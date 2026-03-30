import pandas as pd
import numpy as np
import plotly.graph_objects as go
from scipy.interpolate import griddata

CSV_PATH = "data/processed/spy_iv_surface.csv"
SPOT_PRICE = 634.09

def create_github_ready_surface(df):
    # 1. High-density grid for a "liquid" look
    ti = np.linspace(df['maturity'].min(), df['maturity'].max(), 100)
    ki = np.linspace(df['strike'].min(), df['strike'].max(), 100)
    T, K = np.meshgrid(ti, ki)
    Z = griddata((df['maturity'], df['strike']), df['impliedVol'], (T, K), method='cubic')

    fig = go.Figure()

    # 2. The Main Surface (Add Contours for depth)
    fig.add_trace(go.Surface(
        x=K, y=T, z=Z,
        colorscale='Viridis',
        colorbar=dict(title='IV', thickness=15, len=0.5, x=0.85),
        contours=dict(
            z=dict(show=True, usecolormap=True, highlightcolor="white", project_z=True)
        ),
        opacity=0.9,
        showscale=True
    ))

    # 3. Minimalist Markers (The "Truth" points)
    fig.add_trace(go.Scatter3d(
        x=df['strike'], y=df['maturity'], z=df['impliedVol'],
        mode='markers',
        marker=dict(size=2, color='white', opacity=0.4, line=dict(color='black', width=0.5)),
        name='Market Quotes'
    ))

    # 4. Integrated Spot Price Line (Projected on the Z-floor)
    fig.add_trace(go.Scatter3d(
        x=[SPOT_PRICE, SPOT_PRICE],
        y=[df['maturity'].min(), df['maturity'].max()],
        z=[Z.min(), Z.min()], # Stays on the floor
        mode='lines',
        line=dict(color='red', width=4, dash='dot'),
        name='Current Spot'
    ))

    # 5. Professional "GitHub" Layout
    fig.update_layout(
        title=dict(
            text='<b>SPY Implied Volatility Surface</b><br><span style="font-size:12px">March 27, 2026 Snapshot</span>',
            x=0.05, y=0.95, font=dict(size=20, color="white")
        ),
        template="plotly_dark", # Dark mode looks much better on GitHub
        paper_bgcolor='rgba(0,0,0,0)', # Transparent background
        plot_bgcolor='rgba(0,0,0,0)',
        scene=dict(
            aspectratio=dict(x=1.5, y=1, z=0.7), # Makes the "Strike" axis the focus
            xaxis=dict(title='Strike', gridcolor='dimgray', showbackground=False),
            yaxis=dict(title='Maturity', gridcolor='dimgray', showbackground=False),
            zaxis=dict(title='IV', gridcolor='dimgray', showbackground=False),
            camera=dict(eye=dict(x=1.2, y=1.2, z=0.8))
        ),
        margin=dict(l=0, r=0, b=0, t=0), # Eliminate "empty" space
        legend=dict(yanchor="top", y=0.9, xanchor="left", x=0.1, bgcolor="rgba(0,0,0,0.3)")
    )

    return fig

if __name__ == "__main__":
    df = pd.read_csv(CSV_PATH)
    fig = create_github_ready_surface(df)
    fig.show()