import pandas as pd
import numpy as np

# Create a DataFrame
df = pd.DataFrame({
    'A': [1, 2, 8, 4, 5],
    'B': [np.nan, 2, 3, 4, 5],
    'C': [1, 2, 3, np.nan, 5],
    'D': [1, 2, 3, 4, np.nan]
})

# Calculate the mean of columns including NaN values
df['a_shift'] = df['A'].shift(1)

df['delta'] = df['A'] - df['a_shift']
print(df)

sort_arr = np.sort(np.array(df['delta']))
print('sort arr',sort_arr)
mean_values = df.mean() 

print(mean_values)

