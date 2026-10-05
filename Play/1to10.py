# Take numbers from 1 to 10
numbers = list(range(1, 11))

# Square each number
squared_numbers = [num ** 2 for num in numbers]

# Keep only the ones less than 25
filtered_numbers = [num for num in squared_numbers if num < 25]

# Print the filtered numbers
for number in filtered_numbers:
  print(number)
