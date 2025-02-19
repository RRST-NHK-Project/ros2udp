import matplotlib 
matplotlib.use('Agg')

import matplotlib.pyplot as plt
from matplotlib.patches import Circle, Rectangle, Arc


def draw_court(ax = None):
    if ax is None: 
        fig, ax = plt.subplots(figsize=(8, 7.5))

    
    goal = Circle((0,550), radius=225, linewidth = 2, color = 'black', fill = False)

    three_p_arc = Arc((0,550), 6150,6150, theta1 = 22, theta2 = 158, linewidth = 2, color = 'black')

    three_p_a= Rectangle((2850,1700), 10, -3000, linewidth=2, color = 'black', fill = False)
    three_p_b= Rectangle((-2850,1700), 10, -3000, linewidth=2, color = 'black', fill = False)

    walls = Rectangle((-4000,0), 8000, 7500, linewidth=2, color = 'black', fill = False)


    court_resources=[goal, three_p_arc, three_p_a, three_p_b, walls]

    for resource in court_resources:
        ax.add_patch(resource)
    
    ax.set_xlim(-4000,4000)
    ax.set_ylim(0,7500)
    ax.set_aspect(1)
    ax.axis('off')

    return ax


fig, ax = plt.subplots(figsize=(8,7.5))
draw_court(ax)


plt.savefig('court.png')