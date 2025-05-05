import pandas as pd
import matplotlib.pyplot as plt
import os

data_folder = "../data"

for filename in os.listdir(data_folder):
    data_path = os.path.join(data_folder, filename)
    