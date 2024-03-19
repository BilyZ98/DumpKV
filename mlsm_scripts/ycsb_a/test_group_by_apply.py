import pandas as pd

# Create a DataFrame
df = pd.DataFrame({
    'A': ['foo', 'bar', 'foo', 'bar', 'foo', 'bar', 'foo', 'foo'],
    'B': ['one', 'one', 'two', 'three', 'two', 'two', 'one', 'three'],
    'C': [1, 2, 3, 4, 5, 6, 7, 8],
    'D': [10, 20, 30, 40, 50, 60, 70, 80]
})

# Define a function to calculate the sum of column 'C' and 'D'
def sum_cd(x):
    return x['C'].sum() + x['D'].sum()

# Group the DataFrame by column 'A' and 'B', and apply the function to each group
grouped = df.groupby(['A', 'B']).apply(sum_cd)

print(grouped)
