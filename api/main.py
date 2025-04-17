from fastapi import FastAPI, Query
from pydantic import BaseModel
import subprocess

app = FastAPI()

class PathResponse(BaseModel):
    path: list[str]
    cost: int

@app.get("/shortest-path", response_model=PathResponse)
def get_path(start: str = Query(...), end: str = Query(...)):
    try:
        # Call the compiled C++ executable
        result = subprocess.check_output(["../build/path_example", start, end], text=True)

        # Parse output like:
        # "Shortest path from A to F:\nA -> B -> D -> F\nTotal cost: 10"
        lines = result.strip().splitlines()
        path_line = lines[1].replace(" -> ", ",").strip()
        cost_line = lines[2]

        path = path_line.split(",")
        cost = int(cost_line.split(":")[1].strip())

        return {"path": path, "cost": cost}

    except subprocess.CalledProcessError as e:
        return {"path": [], "cost": -1}
