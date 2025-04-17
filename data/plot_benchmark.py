import pandas as pd
import matplotlib.pyplot as plt

# Load the CSV
df = pd.read_csv("benchmark_results.csv")

# Filter out invalid rows
df = df[(df["cpp_time_ms"] > 0) & (df["mongo_time_ms"] > 0)]

# Plot
plt.figure(figsize=(12, 6))
plt.plot(df.index, df["cpp_time_ms"], label="C++ Pathfinding", marker="o")
plt.plot(df.index, df["mongo_time_ms"], label="MongoDB $graphLookup", marker="x")
plt.xlabel("Query Index")
plt.ylabel("Execution Time (ms)")
plt.title("C++ vs MongoDB Pathfinding Time")
plt.legend()
plt.grid(True)
plt.tight_layout()
plt.savefig("benchmark_timing_comparison.png")
plt.show()
