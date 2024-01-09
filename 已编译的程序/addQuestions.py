
import sqlite3
import random

# Connect to the database
conn = sqlite3.connect('questions.db')

# Create a cursor object
cursor = conn.cursor()

# Add 100 questions to the database
for question_count in range(1, 1001):
    content = f"题目{question_count}"
    answer_A = f"选项{question_count}的A选项"
    answer_B = f"选项{question_count}的B选项"
    answer_C = f"选项{question_count}的C选项"
    answer_D = f"选项{question_count}的D选项"
    answer = random.choice(['A', 'B', 'C', 'D'])

    query = f"INSERT INTO question (content, answer_A, answer_B, answer_C, answer_D, answer) VALUES ('{content}', '{answer_A}', '{answer_B}', '{answer_C}', '{answer_D}', '{answer}')"
    cursor.execute(query)

# Commit the changes
conn.commit()

# Close the cursor and connection
cursor.close()
conn.close()

