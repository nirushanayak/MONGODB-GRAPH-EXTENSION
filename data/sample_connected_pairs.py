from pymongo import MongoClient
import random
import json
from tqdm import tqdm

client = MongoClient()
db = client.graph
edges = db.edges

# Step 1: Pick a random starting node
start_node = edges.aggregate([{"$sample": {"size": 1}}]).next()["from"]
print(f"🚀 Seeding from BFS root: {start_node}")

# Step 2: Run BFS from that node (depth-limited)
reachable = set()
queue = [(start_node, 0)]
max_depth = 6

while queue:
    current, depth = queue.pop(0)
    if depth >= max_depth or current in reachable:
        continue
    reachable.add(current)
    neighbors = edges.find({"from": current}, {"to": 1})
    for n in neighbors:
        to_node = n["to"]
        if to_node not in reachable:
            queue.append((to_node, depth + 1))

print(f"✅ Found {len(reachable)} reachable nodes within depth {max_depth}")

# Step 3: Sample connected pairs from reachable set
reachable = list(reachable)
connected_pairs = [
    (a, b) for a, b in random.sample(
        [(a, b) for a in reachable for b in reachable if a != b],
        100
    )
]

# Step 4: Save to file
with open("data/sample_pairs.json", "w") as f:
    json.dump(connected_pairs, f, indent=2)

print("✅ Saved 100 connected pairs to data/sample_pairs.json")
