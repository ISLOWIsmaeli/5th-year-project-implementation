import matplotlib.pyplot as plt
import numpy as np

# Simulate data
iterations = np.arange(1, 101)
initial_rmse = 1.0
rmse = initial_rmse * np.exp(-0.05 * iterations) + np.random.normal(0, 0.01, size=len(iterations))

# Plotting
plt.figure(figsize=(10, 6))
plt.plot(iterations, rmse, label='RMSE', color='blue', linewidth=2)
plt.title('Expected Output of Adaptive Filtering: RMSE vs Iterations', fontsize=14)
plt.xlabel('Iterations', fontsize=12)
plt.ylabel('Root Mean Square Error (RMSE)', fontsize=12)
plt.grid(True, linestyle='--', alpha=0.7)
plt.legend()
plt.tight_layout()
plt.show()
