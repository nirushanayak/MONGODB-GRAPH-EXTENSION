# insert_weighted_tricky_graph.py

from pymongo import MongoClient
import random

# --- Config ---
client = MongoClient("mongodb://localhost:27017")
db = client["graph"]
edges = db["edges"]

# --- Clean collection ---
edges.delete_many({})
print("🧹 Cleared existing 'edges' collection.")

# --- Insert a tricky weighted graph ---
# Structure:
# A -> B (weight 1)
# A -> C (weight 1000)
# B -> D (weight 1)
# C -> D (weight 1)
# So, A-B-D is cheap, A-C-D is expensive!

docs = [
    {"from": "A", "to": "B", "weight": 1},
    {"from": "A", "to": "C", "weight": 1000},
    {"from": "B", "to": "D", "weight": 1},
    {"from": "C", "to": "D", "weight": 1},
]

edges.insert_many(docs)

print("✅ Inserted weighted tricky graph into MongoDB.")

# --- Summary ---
print("Start Node: A")
print("End Node: D")
