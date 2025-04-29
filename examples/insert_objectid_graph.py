from pymongo import MongoClient
from bson import ObjectId

# --- MongoDB connection ---
client = MongoClient("mongodb://localhost:27017")
db = client["graph"]
edges = db["edges"]

# --- Clear existing data ---
edges.delete_many({})

# --- Create ObjectId-based graph ---
num_nodes = 1000
node_ids = [ObjectId() for _ in range(num_nodes)]

docs = []
for i in range(num_nodes - 1):
    docs.append({
        "from": node_ids[i],
        "to": node_ids[i + 1],
        "weight": 1
    })

edges.insert_many(docs)

print(f"✅ Inserted {len(docs)} edges with ObjectIds into 'graph.edges'.")
print(f"Start Node ID: {node_ids[0]}")
print(f"End Node ID: {node_ids[-1]}")
