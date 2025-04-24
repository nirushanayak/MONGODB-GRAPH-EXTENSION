import pandas as pd
import matplotlib.pyplot as plt

# Load benchmark results
df = pd.read_csv("benchmark_results.csv")

# Filter valid data
df = df[(df["cpp_time_ms"] > 0) & (df["mongo_time_ms"] > 0)]

# === 1. Execution Time Line Plot ===
plt.figure(figsize=(10, 5))
plt.plot(df.index, df["cpp_time_ms"], label="C++ Pathfinding", marker='o')
plt.plot(df.index, df["mongo_time_ms"], label="MongoDB $graphLookup", marker='x')
plt.xlabel("Query Index")
plt.ylabel("Time (ms)")
plt.title("Execution Time: C++ vs MongoDB")
plt.legend()
plt.grid(True)
plt.tight_layout()
plt.savefig("plots/01_execution_time_comparison.png")
plt.close()

# === 4. Execution Time Histograms ===
plt.figure(figsize=(10, 5))
plt.hist(df["cpp_time_ms"], bins=30, alpha=0.7, label="C++", color="green")
plt.hist(df["mongo_time_ms"], bins=30, alpha=0.7, label="MongoDB", color="orange")
plt.xlabel("Execution Time (ms)")
plt.ylabel("Frequency")
plt.title("Execution Time Distribution")
plt.legend()
plt.tight_layout()
plt.savefig("plots/04_execution_time_histogram.png")
plt.close()

print("✅ All benchmark plots saved to: plots/")
