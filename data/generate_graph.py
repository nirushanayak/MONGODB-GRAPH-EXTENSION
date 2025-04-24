from pymongo import MongoClient
import random

client = MongoClient()
db = client.graph
db.edges.drop()

nodes = [f"N{i}" for i in range(1000)]
edges = []

# Fully connected-ish graph: each node connects to 3 others
for node in nodes:
    targets = random.sample([n for n in nodes if n != node], 5)
    for t in targets:
        edges.append({
            "from": node,
            "to": t,
            "weight": random.randint(1, 10)
        })

db.edges.insert_many(edges)
print("Graph populated with 300+ edges")
