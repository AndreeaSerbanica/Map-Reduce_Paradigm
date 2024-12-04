import os

# Iterate through all letters from 'a' to 'z'
for letter in range(ord('a'), ord('z') + 1):
    file_name = f"{chr(letter)}.txt"
    try:
        if os.path.exists(file_name):
            os.remove(file_name)
            print(f"Deleted: {file_name}")
        else:
            print(f"File not found: {file_name}")
    except Exception as e:
        print(f"Error deleting {file_name}: {e}")
