from pymongo import MongoClient
import os
from tqdm import tqdm

print("starting...")
# --- Config ---
dataset_path = "roadNet-CA.txt"
client = MongoClient()
db = client.graph
edges = db.edges

BATCH_SIZE = 50000

# --- Clean existing collection ---
edges.drop()

# --- Load and insert ---
print("📦 Parsing and inserting roadNet-CA...")

docs = []
count = 0

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

        count += 1

        if len(docs) >= BATCH_SIZE:
            edges.insert_many(docs)
            print(f"✅ Inserted {count} edges so far...")
            docs = []  # reset batch

# Insert any remaining documents
if docs:
    edges.insert_many(docs)
    print(f"✅ Inserted final {len(docs)} edges.")

print(f"🏁 Done. Total edges inserted: {count}")
