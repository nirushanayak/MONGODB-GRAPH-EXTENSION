from fastapi import FastAPI, Query
import subprocess

app = FastAPI()

@app.get("/shortest-path")
def get_path(start: str, end: str):
    try:
        result = subprocess.check_output(["../build/weighted_path_example", start, end], text=True)

        if not result.strip():
            return {"path": [], "cost": -1}

        lines = result.strip().splitlines()

        if len(lines) < 3:
            return {"path": [], "cost": -1}

        path_line = lines[1].replace(" -> ", ",").strip()
        cost_line = lines[2]

        if ":" not in cost_line:
            return {"path": [], "cost": -1}

        path = path_line.split(",")
        cost = int(cost_line.split(":")[1].strip())

        return {"path": path, "cost": cost}

    except subprocess.CalledProcessError:
        return {"path": [], "cost": -1}
    except Exception:
        return {"path": [], "cost": -1}
