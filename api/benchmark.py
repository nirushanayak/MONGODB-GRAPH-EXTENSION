import requests
import time
from pymongo import MongoClient
from statistics import mean
import json


# --- Configuration ---
API_URL = "http://localhost:8000/shortest-path"
MONGO_URI = "mongodb://localhost:27017"
GRAPH_DB = "graph"
EDGE_COLLECTION = "edges"
# SAMPLE_NODES = [("N0", "N999"), ("N10000", "N11000"), ("N12345", "N54321")]
with open("data/sample_pairs.json") as f:
    SAMPLE_NODES = json.load(f)

# --- MongoDB Setup ---
client = MongoClient(MONGO_URI)
db = client[GRAPH_DB]
edges = db[EDGE_COLLECTION]

# --- Helper to get path cost from edge list ---
def compute_cost(path):
    cost = 0
    for i in range(len(path) - 1):
        edge = edges.find_one({"from": path[i], "to": path[i+1]})
        if edge:
            cost += edge["weight"]
        else:
            return float("inf")  # incomplete path
    return cost

# --- Run Benchmarks ---
results = []

for start, end in SAMPLE_NODES:
    print(f"▶️  Benchmarking: {start} → {end}")
    # 🔹 Benchmark C++ Engine via API
    t1 = time.time()
    r = requests.get(API_URL, params={"start": start, "end": end})
    t2 = time.time()

    data = r.json()
    cpp_time = round((t2 - t1) * 1000, 2)  # in ms
    cpp_cost = data["cost"]
    cpp_path = data["path"]

    # 🔹 Benchmark MongoDB using $graphLookup
    t3 = time.time()
    agg = list(edges.aggregate([
        { "$match": { "from": start } },
        { "$graphLookup": {
            "from": EDGE_COLLECTION,
            "startWith": "$to",
            "connectFromField": "to",
            "connectToField": "from",
            "as": "path",
            "maxDepth": 10
        }}
    ]))
    t4 = time.time()

    # Approximate result parsing (not optimal)
    mongo_paths = []
    for doc in agg:
        partial = [start] + [p["to"] for p in doc.get("path", [])]
        cost = compute_cost(partial)
        mongo_paths.append((partial, cost))

    mongo_paths = sorted(mongo_paths, key=lambda x: x[1])
    mongo_best_path = mongo_paths[0][0] if mongo_paths else []
    mongo_best_cost = mongo_paths[0][1] if mongo_paths else -1
    mongo_time = round((t4 - t3) * 1000, 2)

    results.append({
        "start": start,
        "end": end,
        "cpp_time_ms": cpp_time,
        "cpp_cost": cpp_cost,
        "cpp_path_len": len(cpp_path),
        "mongo_time_ms": mongo_time,
        "mongo_cost": mongo_best_cost,
        "mongo_path_len": len(mongo_best_path),
        "optimal": "✅" if mongo_best_cost >= cpp_cost else "❌"
    })

# --- Print Results ---
print(f"{'Start':<6} {'End':<6} {'CPP Time':<10} {'Mongo Time':<12} {'CPP Cost':<10} {'Mongo Cost':<12} {'Optimal?'}")
for r in results:
    print(f"{r['start']:<6} {r['end']:<6} {r['cpp_time_ms']:<10} {r['mongo_time_ms']:<12} {r['cpp_cost']:<10} {r['mongo_cost']:<12} {r['optimal']}")

import csv

fieldnames = [
    "start", "end",
    "cpp_time_ms", "mongo_time_ms",
    "cpp_cost", "mongo_cost",
    "cpp_path_len", "mongo_path_len",
    "optimal"
]

with open("benchmark_results.csv", "w", newline="") as f:
    writer = csv.DictWriter(f, fieldnames=fieldnames)
    writer.writeheader()
    for row in results:
        filtered_row = {key: row.get(key, None) for key in fieldnames}
        writer.writerow(filtered_row)

print("✅ CSV written to benchmark_results.csv")



