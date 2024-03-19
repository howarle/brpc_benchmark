import matplotlib.pyplot as plt

parallelisms = [1, 6, 11, 16, 21, 26, 31, 36, 41, 46, 51, 56, 61, ]
attachment_lantencys = dict()
attachment_speed = dict()
proto_lantencys = dict()
proto_speed = dict()
streaming_lantencys = dict()
streaming_speed = dict()

# ================ paste data here begin ================

# attachment  payload size 10240
attachment_lantencys["10k"] = [43.152, 41.017, 59.854, 79.557, 97.192, 97.258, 97.925, 95.552, 91.675, 73.619, 82.495, 98.319, 79.943, ]
attachment_speed["10k"] = [0.440399, 2.64264, 4.79431, 6.89358, 9.03093, 11.2264, 13.3624, 15.5207, 17.6066, 19.8223, 22.0345, 24.0496, 26.4739, ]

# proto  payload size 10240
proto_lantencys["10k"] = [40.87, 41.011, 80.221, 61.232, 78.308, 95.771, 96.334, 98.528, 85.549, 108.916, 85.979, 101.065, 78.787, ]
proto_speed["10k"] = [0.441728, 2.64387, 4.75739, 6.93121, 9.02083, 11.1938, 13.329, 15.4026, 17.7981, 19.7989, 22.0481, 24.0227, 26.3927, ]

# streaming  payload size 10240
streaming_lantencys["10k"] = [41.892, 40.817, 40.78, 60.537, 60.436, 60.658, 60.473, 80.285, 80.275, 60.854, 81.04, 80.985, 81.135, ]
streaming_speed["10k"] = [0.441395, 2.62881, 4.75576, 6.97926, 8.98846, 11.0768, 13.191, 14.9187, 17.1092, 19.0531, 21.077, 23.038, 24.0637, ]

# attachment  payload size 102400
attachment_lantencys["100k"] = [63.889, 122.041, 122.349, 139.583, 141.944, 136.966, 160.191, 154.72, 150.856, 149.121, 149.305, 173.67, 150.823, ]
attachment_speed["100k"] = [4.14602, 24.4942, 44.8018, 64.6553, 86.1716, 106.358, 126.642, 134.347, 133.707, 133.944, 133.327, 132.839, 134.794, ]

# proto  payload size 102400
proto_lantencys["100k"] = [61.751, 121.693, 122.304, 121.659, 138.995, 160.091, 178.309, 155.768, 177.362, 153.362, 122.581, 152.204, 151.189, ]
proto_speed["100k"] = [4.21182, 24.8131, 44.5693, 65.5769, 85.6541, 104.633, 124.5, 134.157, 131.758, 133.961, 132.42, 134.154, 134.57, ]

# streaming  payload size 102400
streaming_lantencys["100k"] = [41.919, 100.711, 120.692, 121.408, 121.377, 122.115, 122.545, 123.877, 125.183, 125.008, 141.491, 120.564, 122.441, ]
streaming_speed["100k"] = [4.27014, 24.5001, 43.887, 60.026, 83.5556, 100.987, 108.145, 116.108, 121.466, 126.477, 128.388, 133.932, 132.301, ]

# attachment  payload size 1048576
attachment_lantencys["1m"] = [127.418, 205.054, 208.705, 270.861, 258.684, 306.017, 360.192, 386.288, 423.239, 452.352, 466.968, 500.751, 488.036, ]
attachment_speed["1m"] = [31.7604, 131.431, 133.32, 132.758, 135.149, 138.165, 137.741, 139.485, 139.641, 141.347, 143.98, 145.674, 146.82, ]

# proto  payload size 1048576
proto_lantencys["1m"] = [147.084, 186.619, 194.036, 252.017, 305.936, 304.702, 341.761, 403.618, 419.582, 434.477, 470.934, 523.808, 520.367, ]
proto_speed["1m"] = [31.1416, 131.684, 134.521, 133.986, 134.575, 137.307, 140.013, 138.583, 140.312, 142.602, 144.199, 143.563, 145.24, ]

# streaming  payload size 1048576
streaming_lantencys["1m"] = [100.466, 160.319, 186.435, 202.712, 250.374, 284.834, 310.036, 359.321, 350.722, 380.61, 412.096, 460.567, 520.133, ]
streaming_speed["1m"] = [40.394, 131.271, 138.997, 142.976, 145.045, 144.972, 151.962, 153.005, 159.805, 162.681, 167.449, 171.57, 172.325, ]

# attachment  payload size 10485760
attachment_lantencys["10m"] = [266.809, 419.287, 897.103, 1145.62, 1551.46, 2021.15, 2361.64, 2553.97, 2918.37, 3300.43, 3610.51, 3851.93, 4301.82, ]
attachment_speed["10m"] = [69.3105, 137.404, 153.108, 165.601, 172.804, 177.168, 181.937, 184.846, 190.161, 195.092, 203.38, 212.625, 210.575, ]

# proto  payload size 10485760
proto_lantencys["10m"] = [253.768, 535.384, 882.742, 1164.05, 1502.58, 1713.06, 2225.2, 2639.3, 2999.16, 3393.63, 3630.58, 3593.15, 4304.18, ]
proto_speed["10m"] = [65.5999, 142.813, 152.59, 163.577, 173.174, 195.456, 181.325, 184.876, 190.115, 194.038, 201.31, 211.449, 207.661, ]


# ================ paste data here end ================


def draw(payload_size):
    # Create a new figure and two subplots
    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(10, 8))

    # Plot array latencies 1 on the first subplot
    ax1.plot(parallelisms, attachment_lantencys[payload_size], marker='o', label='attachment', color='b')
    ax1.plot(parallelisms, proto_lantencys[payload_size], marker='o', label='proto', color='r')
    if(payload_size in streaming_lantencys):
        ax1.plot(parallelisms, streaming_lantencys[payload_size], marker='o', label='streaming', color='r')
    ax1.set_xlabel('Parallelisms')
    ax1.set_ylabel('Latency (ms)')
    ax1.set_title('99% Latency bewteen different method')
    ax1.grid(True)
    ax1.legend()

    # Plot array latencies 2 on the second subplot
    ax2.plot(parallelisms, attachment_speed[payload_size], marker='^', label='attachment', color='b')
    ax2.plot(parallelisms, proto_speed[payload_size], marker='^', label='proto', color='r')
    if(payload_size in streaming_speed):
        ax2.plot(parallelisms, streaming_speed[payload_size], marker='^', label='streaming', color='r')
    ax2.set_xlabel('Parallelisms')
    ax2.set_ylabel('throughput (MB)')
    ax2.set_title('throughput bewteen different method')
    ax2.grid(True)
    ax2.legend()

    # Adjust spacing between subplots
    plt.tight_layout()

    # Show the combined plot
    plt.savefig(f'array_latency_vs_parallelism_{payload_size}.png')

draw("10k")
draw("100k")
draw("1m")
draw("10m")