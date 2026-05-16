import os
import sys
import json
import math
import requests
import networkx as nx
from PIL import Image, ImageDraw
from pyvis.network import Network
from playwright.sync_api import sync_playwright

# =============================
# 全局配置
# =============================
MAX_NAME_LENGTH = 10
MIN_DIST = 140
NODE_SIZE = 40

VIEW_WIDTH = 1920
VIEW_HEIGHT = 1080
DEVICE_SCALE = 2
WAIT_TIME = 800

EDGE_WIDTH = 3
EDGE_OPACITY = 0.85

# 子图之间间距 ⭐
COMPONENT_GAP_X = 400
COMPONENT_GAP_Y = 350

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


# =============================
# 圆形布局
# =============================
def circle_layout(nodes, min_dist):
	n = len(nodes)

	if n == 1:
		return {nodes[0]: (0, 0)}

	radius = max(min_dist * n / (2 * math.pi), 120)

	return {
		node: (
			radius * math.cos(2 * math.pi * i / n),
			radius * math.sin(2 * math.pi * i / n)
		)
		for i, node in enumerate(nodes)
	}


# =============================
# ⭐ 多子图布局（核心优化）
# =============================
def multi_component_layout(G):
	pos = {}

	components = list(nx.weakly_connected_components(G))
	components.sort(key=lambda c: -len(c))

	# ⭐ 存储每个子图的信息
	comp_infos = []

	for comp in components:
		nodes = list(comp)

		sub_pos = circle_layout(nodes, MIN_DIST)

		# ⭐ 计算半径（最大距离）
		max_r = 0
		for x, y in sub_pos.values():
			r = math.sqrt(x * x + y * y)
			max_r = max(max_r, r)

		comp_infos.append({
			"nodes": nodes,
			"pos": sub_pos,
			"radius": max_r + NODE_SIZE * 2  # ⭐ 加上头像尺寸防止贴边
		})

	# =============================
	# ⭐ 动态排版（关键）
	# =============================
	x_cursor = 0
	y_cursor = 0
	row_height = 0

	MAX_WIDTH = 1600  # 一行最大宽度

	for comp in comp_infos:
		r = comp["radius"]
		diameter = r * 2

		# ⭐ 换行判断
		if x_cursor + diameter > MAX_WIDTH:
			x_cursor = 0
			y_cursor += row_height + 80  # 行间距
			row_height = 0

		# ⭐ 放置当前子图
		for node, (x, y) in comp["pos"].items():
			pos[node] = (
				x + x_cursor + r,
				y + y_cursor + r
			)

		# ⭐ 更新游标
		x_cursor += diameter + 80  # 子图间距
		row_height = max(row_height, diameter)

	return pos


# =============================
# 头像处理
# =============================
def get_avatar(qq):
	path = f"avatar_{qq}.png"

	if os.path.exists(path):
		return path

	url = f"https://q.qlogo.cn/g?b=qq&nk={qq}&s=100"

	try:
		r = requests.get(url, timeout=5)
		with open(path, "wb") as f:
			f.write(r.content)

		img = Image.open(path).convert("RGBA")
		size = img.size

		mask = Image.new("L", size, 0)
		draw = ImageDraw.Draw(mask)
		draw.ellipse((0, 0, size[0], size[1]), fill=255)

		img.putalpha(mask)
		img.save(path)

	except Exception:
		img = Image.new("RGBA", (100, 100), (200, 200, 200, 255))
		img.save(path)

	return path


# =============================
# 构建图
# =============================
def build_graph(qq_name, relations):
	G = nx.DiGraph()

	for qq in qq_name.keys():
		qq_name[qq] = qq_name[qq][:MAX_NAME_LENGTH]
	for qq, name in qq_name.items():
		G.add_node(name, image=get_avatar(qq))

	for qq, relation in relations.items():
		src = qq_name[qq]

		# husband → wife（反向）
		if "husband" in relation:
			dst = qq_name[relation["husband"]]
			G.add_edge(dst, src, label="老婆")

		for rel in ["wife", "mother", "master"]:
			if rel in relation:
				dst = qq_name[relation[rel]]
				G.add_edge(src, dst, label=RELATION_MAP[rel])

	return G


# =============================
# 渲染 HTML
# =============================
def render_graph(G, output_html):
	# ⭐ 使用新的布局
	pos = multi_component_layout(G)

	net = Network(height="900px", width="100%", directed=True)

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
			size=NODE_SIZE,
			font={
				"size": 22,
				"strokeWidth": 4,
				"strokeColor": "#ffffff"
			}
		)

	for u, v, attr in G.edges(data=True):
		color = RELATION_COLOR.get(attr["label"], "#999999")

		net.add_edge(
			u,
			v,
			width=EDGE_WIDTH,
			color={
				"color": color,
				"opacity": EDGE_OPACITY
			},
			smooth={
				"enabled": True,
				"type": "curvedCW",
				"roundness": 0.2
			}
		)

	net.set_options("""
	{
	  "physics": { "enabled": false },
	  "edges": {
		"arrows": {
		  "to": {
			"enabled": true,
			"scaleFactor": 0.9
		  }
		}
	  }
	}
	""")

	net.write_html(output_html)

	with open(output_html, "r", encoding="utf-8") as f:
		html = f.read()

	style = """
	<style>
	body {
	  margin: 0;
	  padding: 0;
	  overflow: hidden;
	  background: white;
	}
	</style>
	"""

	legend = """
	<div style="
	position:absolute;
	top:20px;
	left:20px;
	background:rgba(255,255,255,0.95);
	padding:12px 16px;
	border-radius:10px;
	box-shadow:0 2px 8px rgba(0,0,0,0.15);
	font-size:18px;
	z-index:999;
	">
	<b>关系说明</b>
	"""

	for name, color in RELATION_COLOR.items():
		legend += f"""
		<div style="margin-top:6px;">
			<span style="
				display:inline-block;
				width:22px;
				height:4px;
				background:{color};
				margin-right:8px;
			"></span>
			{name}
		</div>
		"""

	legend += "</div>"

	html = html.replace("</head>", style + "</head>")
	html = html.replace("<body>", "<body>" + legend)

	with open(output_html, "w", encoding="utf-8") as f:
		f.write(html)


# =============================
# HTML → PNG
# =============================
def html_to_png(html_file, output_png):
	with sync_playwright() as p:
		browser = p.chromium.launch(headless=True)

		page = browser.new_page(
			viewport={"width": VIEW_WIDTH, "height": VIEW_HEIGHT},
			device_scale_factor=DEVICE_SCALE
		)

		page.goto(
			"file://" + os.path.abspath(html_file),
			wait_until="domcontentloaded"
		)

		page.wait_for_selector(".vis-network", timeout=5000)
		page.wait_for_timeout(WAIT_TIME)

		page.locator(".vis-network").screenshot(path=output_png)

		browser.close()


# =============================
# 主函数
# =============================
def main():
	qq_name_json = sys.argv[1]
	relations_json = sys.argv[2]
	output_png = sys.argv[3]

	with open(qq_name_json, "r", encoding="utf-8") as f:
		qq_name = json.load(f)

	with open(relations_json, "r", encoding="utf-8") as f:
		relations = json.load(f)

	G = build_graph(qq_name, relations)

	html_file = "graph.html"
	render_graph(G, html_file)
	html_to_png(html_file, output_png)


if __name__ == "__main__":
	main()
