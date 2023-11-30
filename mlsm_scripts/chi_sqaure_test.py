import scipy.stats as stats

# Let's assume we have two categorical variables.
# We will represent them as lists for this example.
var1 = [20, 15, 25, 40]
var2 = [25, 15, 30, 30]

# Perform the Chi-squared test
chi2, p, dof, expected = stats.chi2_contingency([var1, var2])

print(f"Chi2 value: {chi2}")
print(f"P-value: {p}")
print(f"Degrees of freedom: {dof}")
print(f"Expected contingency table: {expected}")

