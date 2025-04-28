# load_roadnet_to_mongo.py

import pymongo
import random

MONGO_URI = "mongodb://localhost:27017"
DB_NAME = "graph"
COLLECTION_NAME = "edges"

client = pymongo.MongoClient(MONGO_URI)
db = client[DB_NAME]
edges = db[COLLECTION_NAME]

# Clear previous data
edges.delete_many({})

with open("/Users/nirushanayak/dev/PROJECTS/ADS/mongodb-graph-extension/data/roadNet-CA.txt", "r") as f:
    for line in f:
        if line.startswith("#"):
            continue
        from_node, to_node = map(int, line.strip().split())

        doc = {
            "from": f"N{from_node}",
            "to": f"N{to_node}",
            "weight": random.randint(1, 10)  # Assign random weight between 1 and 10
        }
        edges.insert_one(doc)

print("✅ RoadNet-CA loaded into MongoDB with random weights.")
