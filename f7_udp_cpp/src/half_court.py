import matplotlib
matplotlib.use('Agg')

import matplotlib.pyplot as plt
import scipy as sp
import numpy as np
import pandas as pd
from scipy.stats import gaussian_kde
from matplotlib.patches import Circle, Rectangle, Arc


def draw_court(ax = None):
    if ax is None: 
        fig, ax = plt.subplots(figsize=(8, 7.5))

    goal = Circle((0,550), radius=225, linewidth = 2, color = 'black', fill = False)
    three_p_arc = Arc((0,550), 6150, 6150, theta1 = 22, theta2 = 158, linewidth = 2, color = 'black')
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

 
def heatmap(data, ax=None, color_map='hot'):
    x_pose = data['X']
    y_pose = data['Y']

    positions = np.vstack([x_pose,y_pose])
    kde = gaussian_kde(positions)

    x_grid = np.linspace(-4000,4000, 100)
    y_grid = np.linspace(0, 7500, 100)

    x_grid , y_grid = np.meshgrid(x_grid, y_grid)

    grid_coords = np.vstack([x_grid.ravel(),y_grid.ravel()])

    result = np.reshape(kde(grid_coords).T ,x_grid.shape)

    ax.contourf(x_grid, y_grid, result, levels=10, cmap=color_map, alpha=0.6)

    return ax


data = pd.read_csv('mr_parameter.csv')

fig, ax = plt.subplots(figsize=(8,7.5))
draw_court(ax)
heatmap(data, ax=ax)

plt.savefig('court.png')
