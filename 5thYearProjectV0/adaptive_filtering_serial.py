import serial
import matplotlib.pyplot as plt
from collections import deque
import matplotlib.animation as animation

# Serial configuration (update COM port as needed)
SERIAL_PORT = 'COM11'  # Change to your ESP32's COM port
BAUD_RATE = 115200

# Initialize plot
plt.style.use('ggplot')
fig, ax = plt.subplots(figsize=(12, 6))
xs = deque(maxlen=1000)  # Store last 1000 iterations
ys = deque(maxlen=1000)  # Store last 1000 RMSE values
line, = ax.plot([], [], 'b-', linewidth=1.5, alpha=0.8)
ax.set_title('LMS Adaptive Filter Convergence', fontsize=14)
ax.set_xlabel('Iteration', fontsize=12)
ax.set_ylabel('RMSE', fontsize=12)
ax.grid(True, alpha=0.3)
fig.tight_layout()

# Serial connection with error handling
try:
    ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
    print(f"Connected to {SERIAL_PORT} at {BAUD_RATE} baud")
except serial.SerialException as e:
    print(f"Failed to connect to {SERIAL_PORT}: {e}")
    exit()

def init():
    line.set_data([], [])
    return line,

def update(frame):
    while ser.in_waiting:
        try:
            line_str = ser.readline().decode('utf-8').strip()
            
            # Skip header and empty lines
            if not line_str or line_str.startswith("Time"):
                return line,
                
            # Parse CSV data
            parts = line_str.split(',')
            if len(parts) == 3:
                time_ms, iteration, rmse = parts
                xs.append(int(iteration))
                ys.append(float(rmse))
                
                # Update plot
                line.set_data(xs, ys)
                ax.relim()
                ax.autoscale_view(scalex=True, scaley=True)
                
        except UnicodeDecodeError:
            print("Warning: Dropped corrupted serial data")
        except ValueError:
            print(f"Warning: Couldn't parse line: {line_str}")
        except Exception as e:
            print(f"Error: {e}")
            
    return line,

try:
    # Clear any initial buffered data
    ser.reset_input_buffer()
    
    # Start animation
    ani = animation.FuncAnimation(
        fig, 
        update, 
        init_func=init,
        interval=50,  # Update interval in ms
        blit=True,
        cache_frame_data=False
    )
    
    plt.show()
    
except KeyboardInterrupt:
    print("\nPlotting stopped by user")
finally:
    ser.close()
    print("Serial connection closed")