# relation_graph_final.py
# 关系图最终版（骨架重构版）
# 功能：
# - 圆形头像
# - 关系颜色图例
# - 核心节点+叶子节点两阶段布局
# - 最大连通子图居中
# - 其他子图环绕布局
# - Playwright截图

import os
import sys
import json
import math
import requests
import networkx as nx
from PIL import Image, ImageDraw
from pyvis.network import Network
from playwright.sync_api import sync_playwright

MAX_NAME_LENGTH = 10
MIN_DIST = 140
NODE_SIZE = 40

VIEW_WIDTH = 1920
VIEW_HEIGHT = 1080
DEVICE_SCALE = 2
WAIT_TIME = 800

EDGE_WIDTH = 3

RELATION_MAP = {
    "wife": "老婆",
    "mother": "妈妈",
    "master": "主人"
}

RELATION_COLOR = {
    "老婆": "#ff4d6d",
    "妈妈": "#51cf66",
    "主人": "#ffd43b"
}


def get_avatar(qq):
    path = f"avatar_{qq}.png"
    if os.path.exists(path):
        return path

    try:
        r = requests.get(
            f"https://q.qlogo.cn/g?b=qq&nk={qq}&s=100",
            timeout=5
        )
        with open(path, "wb") as f:
            f.write(r.content)

        img = Image.open(path).convert("RGBA")
        mask = Image.new("L", img.size, 0)
        ImageDraw.Draw(mask).ellipse(
            (0, 0, img.size[0], img.size[1]),
            fill=255
        )
        img.putalpha(mask)
        img.save(path)

    except Exception:
        Image.new(
            "RGBA",
            (100, 100),
            (200, 200, 200, 255)
        ).save(path)

    return path


def component_layout(subgraph):
    nodes = list(subgraph.nodes)

    core_nodes = []
    leaf_nodes = []

    for node in nodes:
        if subgraph.degree(node) <= 1:
            leaf_nodes.append(node)
        else:
            core_nodes.append(node)

    if not core_nodes:
        core_nodes = nodes
        leaf_nodes = []

    pos = {}

    radius = max(
        MIN_DIST * len(core_nodes) / (2 * math.pi),
        120
    )

    for i, node in enumerate(core_nodes):
        a = 2 * math.pi * i / len(core_nodes)
        pos[node] = (
            radius * math.cos(a),
            radius * math.sin(a)
        )

    parent_children = {}

    for node in leaf_nodes:
        neighbors = (
            list(subgraph.predecessors(node))
            + list(subgraph.successors(node))
        )

        if not neighbors:
            continue

        parent_children.setdefault(neighbors[0], []).append(node)

    LEAF_DISTANCE = 180

    for parent, children in parent_children.items():
        px, py = pos[parent]
        base = math.atan2(py, px)

        for i, child in enumerate(children):
            if len(children) == 1:
                angle = base
            else:
                angle = base + (
                    (i - (len(children)-1)/2)
                    * math.pi/2
                    / (len(children)-1)
                )

            pos[child] = (
                px + LEAF_DISTANCE * math.cos(angle),
                py + LEAF_DISTANCE * math.sin(angle)
            )

    return pos


def multi_component_layout(G):
    comps = list(nx.weakly_connected_components(G))
    comps.sort(key=len, reverse=True)

    pos = {}

    if not comps:
        return pos

    # 最大子图居中
    main = G.subgraph(comps[0])
    main_pos = component_layout(main)

    for k, v in main_pos.items():
        pos[k] = v

    # 其余子图环绕
    others = comps[1:]
    ring_radius = 800

    for idx, comp in enumerate(others):
        angle = 2 * math.pi * idx / max(len(others), 1)

        cx = ring_radius * math.cos(angle)
        cy = ring_radius * math.sin(angle)

        sub = G.subgraph(comp)
        sub_pos = component_layout(sub)

        for node, (x, y) in sub_pos.items():
            pos[node] = (x + cx, y + cy)
    pos = avoid_overlap(pos, min_dist=140)
    return pos


def compute_canvas_size(pos, padding=300):
    xs = [x for x, y in pos.values()]
    ys = [y for x, y in pos.values()]

    min_x, max_x = min(xs), max(xs)
    min_y, max_y = min(ys), max(ys)

    width = int(max_x - min_x + padding)
    height = int(max_y - min_y + padding)

    return max(width, 800), max(height, 600)


def normalize_pos(pos):
    xs = [x for x, y in pos.values()]
    ys = [y for x, y in pos.values()]

    min_x = min(xs)
    min_y = min(ys)

    return {
        n: (x - min_x + 150, y - min_y + 150)
        for n, (x, y) in pos.items()
    }


def build_graph(qq_name, relations):
    G = nx.DiGraph()

    for qq, name in qq_name.items():
        name = name[:MAX_NAME_LENGTH]
        G.add_node(name, image=get_avatar(qq))
        qq_name[qq] = name

    for qq, relation in relations.items():
        src = qq_name[qq]

        if "husband" in relation:
            dst = qq_name[relation["husband"]]
            G.add_edge(dst, src, label="老婆")

        for rel in ("wife", "mother", "master"):
            if rel in relation:
                dst = qq_name[relation[rel]]
                G.add_edge(src, dst, label=RELATION_MAP[rel])

    return G


def render_graph(G, html_file):
    pos = multi_component_layout(G)
    pos = normalize_pos(pos)
    canvas_w, canvas_h = compute_canvas_size(pos)
    net = Network(
        height=f"{canvas_h}px",
        width=f"{canvas_w}px",
        directed=True
    )

    for node, attr in G.nodes(data=True):
        x, y = pos[node]

        net.add_node(
            node,
            label=node,
            image=attr["image"],
            shape="circularImage",
            x=x,
            y=y,
            fixed=True,
            size=NODE_SIZE
        )

    for u, v, attr in G.edges(data=True):
        net.add_edge(
            u,
            v,
            width=EDGE_WIDTH,
            color=RELATION_COLOR.get(attr["label"], "#999999"),
            smooth={
                "enabled": True,
                "type": "curvedCW",
                "roundness": 0.2
            }
        )

    net.set_options("""
    {
      "physics":{"enabled":false}
    }
    """)
    legend_html = """
    <div style="
    position: fixed;
    top: 20px;
    right: 20px;
    background: rgba(255,255,255,0.9);
    padding: 10px;
    border-radius: 8px;
    font-size: 14px;
    z-index: 9999;
    ">
    <b>关系图例</b><br>
    <span style="color:#ff4d6d;">●</span> 老婆<br>
    <span style="color:#51cf66;">●</span> 妈妈<br>
    <span style="color:#ffd43b;">●</span> 主人<br>
    </div>
    """

    net.html = net.html.replace("</body>", legend_html + "</body>")
    html_raw = net.generate_html()

    legend_html = """
    <div style="
    position: fixed;
    top: 20px;
    right: 20px;
    background: rgba(255,255,255,0.92);
    padding: 10px;
    border-radius: 8px;
    font-size: 14px;
    z-index: 9999;
    box-shadow: 0 2px 10px rgba(0,0,0,0.2);
    ">
    <b>关系图例</b><br>
    <span style="color:#ff4d6d;">●</span> 老婆<br>
    <span style="color:#51cf66;">●</span> 妈妈<br>
    <span style="color:#ffd43b;">●</span> 主人<br>
    </div>
    """

    html_raw = html_raw.replace("</body>", legend_html + "</body>")

    with open(html_file, "w", encoding="utf-8") as f:
        f.write(html_raw)


def avoid_overlap(pos, min_dist=120, iterations=50):
    nodes = list(pos.keys())

    for _ in range(iterations):
        moved = False

        for i in range(len(nodes)):
            for j in range(i + 1, len(nodes)):
                n1, n2 = nodes[i], nodes[j]
                x1, y1 = pos[n1]
                x2, y2 = pos[n2]

                dx = x2 - x1
                dy = y2 - y1
                dist = math.hypot(dx, dy)

                if dist < min_dist and dist > 0:
                    move = (min_dist - dist) / 2

                    dx /= dist
                    dy /= dist

                    pos[n1] = (x1 - dx * move, y1 - dy * move)
                    pos[n2] = (x2 + dx * move, y2 + dy * move)

                    moved = True

        if not moved:
            break

    return pos


def html_to_png(html_file, output_png):
    with sync_playwright() as p:
        browser = p.chromium.launch(headless=True)

        page = browser.new_page(
            viewport={
                "width": VIEW_WIDTH,
                "height": VIEW_HEIGHT
            },
            device_scale_factor=DEVICE_SCALE
        )

        page.goto(
            "file://" + os.path.abspath(html_file),
            wait_until="domcontentloaded"
        )

        page.wait_for_timeout(WAIT_TIME)
        page.locator(".vis-network").screenshot(
            path=output_png
        )

        browser.close()


def main():
    qq_json = sys.argv[1]
    rel_json = sys.argv[2]
    out_png = sys.argv[3]

    with open(qq_json, "r", encoding="utf-8") as f:
        qq_name = json.load(f)

    with open(rel_json, "r", encoding="utf-8") as f:
        relations = json.load(f)

    G = build_graph(qq_name, relations)

    html = "graph.html"
    render_graph(G, html)
    html_to_png(html, out_png)


if __name__ == "__main__":
    main()
