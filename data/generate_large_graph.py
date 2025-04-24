from pymongo import MongoClient
import random
from tqdm import tqdm

# --- Config ---
NUM_NODES = 10_000
AVG_EDGES_PER_NODE = 6
MAX_WEIGHT = 20

# --- Connect to MongoDB ---
client = MongoClient()
db = client.graph
edges = db.edges

# Clear existing data
edges.drop()

# Generate nodes
nodes = [f"N{i}" for i in range(NUM_NODES)]

# Generate edges
edge_docs = []
print(f"📦 Generating ~{NUM_NODES * AVG_EDGES_PER_NODE} edges...")
for node in tqdm(nodes):
    targets = random.sample([n for n in nodes if n != node], AVG_EDGES_PER_NODE)
    for target in targets:
        edge_docs.append({
            "from": node,
            "to": target,
            "weight": random.randint(1, MAX_WEIGHT)
        })

# Insert into MongoDB
print(f"⬆️ Inserting {len(edge_docs)} edges into MongoDB...")
edges.insert_many(edge_docs)
print(f"✅ Done! Created graph with {NUM_NODES} nodes and {len(edge_docs)} edges.")
