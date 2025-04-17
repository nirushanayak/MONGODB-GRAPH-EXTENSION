from pymongo import MongoClient
import os

# --- Config ---
dataset_path = "data/wiki-Vote.txt"  # Downloaded SNAP file
client = MongoClient()
db = client.graph
edges = db.edges

# --- Clear old ---
edges.drop()

# --- Parse and insert ---
docs = []
with open(dataset_path) as f:
    for line in f:
        if line.startswith("#"):
            continue
        parts = line.strip().split()
        if len(parts) != 2:
            continue
        docs.append({
            "from": f"N{parts[0]}",
            "to": f"N{parts[1]}",
            "weight": 1  # SNAP doesn't include weights, so we assign uniform cost
        })

# --- Bulk insert ---
print(f"Inserting {len(docs)} edges...")
edges.insert_many(docs)
print("✅ SNAP dataset loaded into MongoDB.")
