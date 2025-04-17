from pymongo import MongoClient
import os
from tqdm import tqdm

# --- Config ---
dataset_path = "data/roadNet-CA.txt"
client = MongoClient()
db = client.graph
edges = db.edges

# --- Clean existing collection ---
edges.drop()

# --- Load and insert ---
print("📦 Parsing roadNet-CA...")
docs = []
with open(dataset_path) as f:
    for line in tqdm(f):
        if line.startswith("#"):
            continue
        parts = line.strip().split()
        if len(parts) != 2:
            continue
        docs.append({
            "from": f"N{parts[0]}",
            "to": f"N{parts[1]}",
            "weight": 1
        })

print(f"⬆️ Inserting {len(docs)} edges into MongoDB...")
edges.insert_many(docs)
print("✅ Loaded roadNet-CA into MongoDB.")
