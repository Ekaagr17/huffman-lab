const TreeVisualizer = {
  treeData: null,
  treeLayout: null,
  currentFilename: 'file',
  
  scale: 1,
  panX: 0,
  panY: 0,
  isPanning: false,
  startX: 0,
  startY: 0,

  canvas: null,
  ctx: null,
  nodeMap: new Map(),

  BOX_W: 54,
  BOX_H: 34,
  Y_STEP: 86,
  LEAF_GAP: 72,

  init() {
    this.canvas = document.getElementById('tree-canvas');
    if (!this.canvas) return;
    this.ctx = this.canvas.getContext('2d');

    this.resizeCanvas();
    window.addEventListener('resize', () => {
      this.resizeCanvas();
      this.draw();
    });

    if (window.ResizeObserver && this.canvas.parentElement) {
      const ro = new ResizeObserver(() => {
        if (this.canvas.parentElement.clientWidth > 50) {
          this.resizeCanvas();
          this.draw();
        }
      });
      ro.observe(this.canvas.parentElement);
    }

    this.setupEvents();
  },

  resizeCanvas() {
    if (!this.canvas) return;
    const container = this.canvas.parentElement;
    const rect = container ? container.getBoundingClientRect() : null;
    
    let w = (rect && rect.width > 50) ? rect.width : (window.innerWidth ? Math.max(800, window.innerWidth - 320) : 960);
    let h = (rect && rect.height > 50) ? rect.height : 560;

    this.canvas.width = Math.round(w);
    this.canvas.height = Math.round(h);
  },

  setupEvents() {
    const stage = document.getElementById('tree-stage');
    if (!stage) return;

    stage.addEventListener('mousedown', (e) => {
      this.isPanning = true;
      this.startX = e.clientX - this.panX;
      this.startY = e.clientY - this.panY;
      stage.style.cursor = 'grabbing';
    });

    window.addEventListener('mousemove', (e) => {
      if (!this.isPanning) return;
      this.panX = e.clientX - this.startX;
      this.panY = e.clientY - this.startY;
      this.draw();
    });

    window.addEventListener('mouseup', () => {
      if (this.isPanning) {
        this.isPanning = false;
        stage.style.cursor = 'grab';
      }
    });

    stage.addEventListener('wheel', (e) => {
      e.preventDefault();
      const zoomFactor = e.deltaY < 0 ? 1.15 : 0.85;
      const rect = this.canvas.getBoundingClientRect();
      const mouseX = e.clientX - rect.left;
      const mouseY = e.clientY - rect.top;
      this.zoomAt(zoomFactor, mouseX, mouseY);
    }, { passive: false });

    document.getElementById('viz-zoom-in')?.addEventListener('click', () => this.zoom(1.2));
    document.getElementById('viz-zoom-out')?.addEventListener('click', () => this.zoom(0.8));
    document.getElementById('viz-fit')?.addEventListener('click', () => this.fitView());

    document.getElementById('btn-download-tree-png')?.addEventListener('click', () => this.exportPNG());
    document.getElementById('btn-download-tree-svg')?.addEventListener('click', () => this.exportSVG());
    document.getElementById('btn-results-download-tree')?.addEventListener('click', () => this.exportPNG());
  },

  loadTree(data, filename) {
    if (!data) return;
    this.treeData = data;
    this.currentFilename = filename || this.currentFilename || 'data';
    this.nodeMap.clear();

    if (data.nodes) {
      for (const n of data.nodes) {
        this.nodeMap.set(n.id, n);
      }
    }

    const rootHierarchy = data.hierarchy || this.buildHierarchyFromNodes(data.root_id);
    const totalFreq = data.total_freq || (rootHierarchy ? rootHierarchy.freq : 1);

    this.treeLayout = this.buildLayout(rootHierarchy, totalFreq);
    this.renderCodebook(data.codes);
    this.fitView();
  },

  buildHierarchyFromNodes(rootId) {
    const node = this.nodeMap.get(rootId);
    if (!node) return null;

    const build = (n, edge) => {
      const item = {
        id: n.id,
        freq: n.freq,
        is_leaf: n.is_leaf,
        label: n.label,
        symbol: n.symbol,
        edge: edge,
        children: []
      };
      if (!n.is_leaf) {
        if (n.left_id !== null && n.left_id !== undefined) {
          const leftNode = this.nodeMap.get(n.left_id);
          if (leftNode) item.children.push(build(leftNode, '0'));
        }
        if (n.right_id !== null && n.right_id !== undefined) {
          const rightNode = this.nodeMap.get(n.right_id);
          if (rightNode) item.children.push(build(rightNode, '1'));
        }
      }
      return item;
    };

    return build(node, null);
  },

  buildLayout(root, totalFreq) {
    if (!root) return null;

    const leaves = [];
    const collectLeaves = (u) => {
      if (!u) return;
      if (u.is_leaf || !u.children || u.children.length === 0) {
        leaves.push(u);
        return;
      }
      if (u.children[0]) collectLeaves(u.children[0]);
      if (u.children[1]) collectLeaves(u.children[1]);
    };
    collectLeaves(root);

    const numLeaves = leaves.length;
    const leafGap = this.LEAF_GAP;

    const leafXMap = new Map();
    leaves.forEach((leaf, idx) => {
      leafXMap.set(leaf.id, idx * leafGap);
    });

    let minX = Infinity;
    let maxX = -Infinity;
    let maxY = 0;

    const assignCoords = (u, depth, isLeftChild, isRoot = false) => {
      if (!u) return null;

      const isLeaf = u.is_leaf || !u.children || u.children.length === 0;
      const y = depth * this.Y_STEP;
      let x = 0;

      let leftLayout = null;
      let rightLayout = null;

      if (isLeaf) {
        x = leafXMap.has(u.id) ? leafXMap.get(u.id) : 0;
      } else {
        if (u.children && u.children.length > 0) {
          if (u.children[0]) leftLayout = assignCoords(u.children[0], depth + 1, true, false);
          if (u.children[1]) rightLayout = assignCoords(u.children[1], depth + 1, false, false);
        }

        if (leftLayout && rightLayout) {
          x = (leftLayout.x + rightLayout.x) / 2;
        } else if (leftLayout) {
          x = leftLayout.x;
        } else if (rightLayout) {
          x = rightLayout.x;
        }
      }

      if (x < minX) minX = x;
      if (x > maxX) maxX = x;
      const bottom = y + (isLeaf ? 58 : 20);
      if (bottom > maxY) maxY = bottom;

      return {
        node: u,
        x: x,
        y: y,
        isRoot: isRoot,
        bit: isRoot ? null : (isLeftChild ? '0' : '1'),
        isLeaf: isLeaf,
        left: leftLayout,
        right: rightLayout
      };
    };

    const rootLayout = assignCoords(root, 0, false, true);

    return {
      root: rootLayout,
      minX: minX === Infinity ? 0 : minX,
      maxX: maxX === -Infinity ? 0 : maxX,
      minY: 0,
      maxY: maxY,
      width: Math.max(100, (maxX - minX) + this.BOX_W + 60),
      height: Math.max(100, maxY + 60),
      totalFreq: totalFreq || (root.freq || 1),
      numLeaves: numLeaves
    };
  },

  zoom(factor) {
    const cx = (this.canvas ? this.canvas.width : 900) / 2;
    const cy = (this.canvas ? this.canvas.height : 560) / 2;
    this.zoomAt(factor, cx, cy);
  },

  zoomAt(factor, cx, cy) {
    const newScale = Math.max(0.08, Math.min(3.0, this.scale * factor));
    this.panX = cx - (cx - this.panX) * (newScale / this.scale);
    this.panY = cy - (cy - this.panY) * (newScale / this.scale);
    this.scale = newScale;
    this.draw();
  },

  fitView() {
    this.resizeCanvas();
    const canvasW = this.canvas ? this.canvas.width : 960;
    const canvasH = this.canvas ? this.canvas.height : 560;

    if (!this.treeLayout || !this.treeLayout.root) {
      this.scale = 1;
      this.panX = canvasW / 2;
      this.panY = 40;
      this.draw();
      return;
    }

    const treeW = this.treeLayout.width;
    const treeH = this.treeLayout.height;

    const scaleX = (canvasW - 60) / treeW;
    const scaleY = (canvasH - 80) / treeH;
    let targetScale = Math.min(scaleX, scaleY);

    targetScale = Math.max(0.12, Math.min(1.15, targetScale));
    if (isNaN(targetScale) || targetScale <= 0) targetScale = 1;

    this.scale = targetScale;
    const treeCenterX = (this.treeLayout.minX + this.treeLayout.maxX) / 2;
    this.panX = canvasW / 2 - treeCenterX * this.scale;
    this.panY = 45;

    this.draw();
  },

  draw() {
    if (!this.ctx || !this.canvas) return;
    const ctx = this.ctx;
    ctx.clearRect(0, 0, this.canvas.width, this.canvas.height);

    ctx.fillStyle = '#faf8f5';
    ctx.fillRect(0, 0, this.canvas.width, this.canvas.height);

    if (!this.treeLayout || !this.treeLayout.root) {
      ctx.fillStyle = '#65717D';
      ctx.font = '14px -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif';
      ctx.textAlign = 'center';
      ctx.fillText('No tree data available. Upload or select a file to generate the Huffman Tree.', this.canvas.width / 2, this.canvas.height / 2);
      return;
    }

    ctx.save();
    ctx.translate(this.panX, this.panY);
    ctx.scale(this.scale, this.scale);

    this.renderSubtree(ctx, this.treeLayout.root, this.treeLayout.totalFreq);

    ctx.restore();
  },

  renderSubtree(ctx, layoutNode, totalFreq) {
    if (!layoutNode) return;

    const HALF_H = this.BOX_H / 2;

    if (layoutNode.left) {
      ctx.beginPath();
      ctx.moveTo(layoutNode.x, layoutNode.y + HALF_H);
      ctx.lineTo(layoutNode.left.x, layoutNode.left.y - HALF_H);
      ctx.strokeStyle = '#1a1a1a';
      ctx.lineWidth = 1.4;
      ctx.stroke();

      this.renderSubtree(ctx, layoutNode.left, totalFreq);
    }

    if (layoutNode.right) {
      ctx.beginPath();
      ctx.moveTo(layoutNode.x, layoutNode.y + HALF_H);
      ctx.lineTo(layoutNode.right.x, layoutNode.right.y - HALF_H);
      ctx.strokeStyle = '#1a1a1a';
      ctx.lineWidth = 1.4;
      ctx.stroke();

      this.renderSubtree(ctx, layoutNode.right, totalFreq);
    }

    this.drawNode(ctx, layoutNode, totalFreq);
  },

  drawNode(ctx, layoutNode, totalFreq) {
    const { x, y, node, isRoot, bit, isLeaf } = layoutNode;
    const BOX_W = this.BOX_W;
    const BOX_H = this.BOX_H;
    const HALF_W = BOX_W / 2;
    const HALF_H = BOX_H / 2;

    ctx.fillStyle = '#FFFFFF';
    ctx.fillRect(x - HALF_W, y - HALF_H, BOX_W, BOX_H);
    ctx.strokeStyle = '#1a1a1a';
    ctx.lineWidth = 1.4;
    ctx.strokeRect(x - HALF_W, y - HALF_H, BOX_W, BOX_H);

    ctx.beginPath();
    ctx.moveTo(x - HALF_W, y);
    ctx.lineTo(x + HALF_W, y);
    ctx.stroke();

    let probStr = '0.0000';
    if (isRoot) {
      probStr = '1.0000';
    } else if (totalFreq > 0) {
      probStr = (node.freq / totalFreq).toFixed(4);
    } else {
      probStr = (node.freq || 0).toString();
    }

    ctx.fillStyle = '#1a1a1a';
    ctx.font = '10px "Courier New", Courier, monospace';
    ctx.textAlign = 'center';
    ctx.textBaseline = 'middle';
    ctx.fillText(probStr, x, y - 8.5);

    if (isRoot) {
      ctx.font = 'bold 9px Arial, sans-serif';
      ctx.fillText('ROOT', x, y + 8.5);
    } else {
      ctx.font = 'bold 12px "Courier New", Courier, monospace';
      ctx.fillText(bit || '0', x, y + 8.5);
    }

    if (isLeaf) {
      const stemLength = 8;
      const circleRadius = 12;
      const circleY = y + HALF_H + stemLength + circleRadius;

      ctx.beginPath();
      ctx.moveTo(x, y + HALF_H);
      ctx.lineTo(x, y + HALF_H + stemLength);
      ctx.strokeStyle = '#1a1a1a';
      ctx.lineWidth = 1.4;
      ctx.stroke();

      ctx.beginPath();
      ctx.arc(x, circleY, circleRadius, 0, Math.PI * 2);
      ctx.fillStyle = '#FFFFFF';
      ctx.fill();
      ctx.strokeStyle = '#1a1a1a';
      ctx.lineWidth = 1.4;
      ctx.stroke();

      const glyph = this.formatSymbolGlyph(node);
      ctx.fillStyle = '#1a1a1a';
      ctx.font = glyph.length > 2 ? 'bold 8.5px Arial, sans-serif' : 'bold 11px Arial, sans-serif';
      ctx.fillText(glyph, x, circleY);
    }
  },

  formatSymbolGlyph(node) {
    const sym = node.symbol;
    if (sym === undefined || sym === null) {
      if (node.label && node.label.startsWith("'") && node.label.endsWith("'")) {
        return node.label.slice(1, -1);
      }
      return node.label || '?';
    }
    if (sym === 32) return 'SP';
    if (sym === 10) return '\\n';
    if (sym === 13) return '\\r';
    if (sym === 9) return '\\t';
    if (sym === 0) return '\\0';
    if (sym >= 33 && sym <= 126) {
      return String.fromCharCode(sym);
    }
    return sym.toString(16).toUpperCase().padStart(2, '0');
  },

  exportPNG() {
    if (!this.treeLayout || !this.treeLayout.root) {
      App.showToast('No tree data available to export.', 'error');
      return;
    }

    const padding = 50;
    const dpr = 2;
    const treeW = Math.max(200, this.treeLayout.maxX - this.treeLayout.minX + this.BOX_W + 40);
    const treeH = Math.max(200, this.treeLayout.maxY + 50);

    const width = Math.round(treeW + padding * 2);
    const height = Math.round(treeH + padding * 2);

    const offscreen = document.createElement('canvas');
    offscreen.width = width * dpr;
    offscreen.height = height * dpr;
    const oCtx = offscreen.getContext('2d');

    oCtx.scale(dpr, dpr);
    oCtx.fillStyle = '#FFFFFF';
    oCtx.fillRect(0, 0, width, height);

    oCtx.fillStyle = '#202A35';
    oCtx.font = 'bold 14px Arial, sans-serif';
    oCtx.textAlign = 'left';
    oCtx.fillText(`HUFFMAN LAB - Tree Diagram: ${this.currentFilename}`, padding, 28);
    oCtx.font = '11px Arial, sans-serif';
    oCtx.fillStyle = '#65717D';
    oCtx.fillText(`Total Bytes: ${this.treeLayout.totalFreq} | Unique Symbols: ${this.treeLayout.numLeaves}`, padding, 44);

    const offsetX = padding - this.treeLayout.minX + 20;
    const offsetY = padding + 30;
    oCtx.translate(offsetX, offsetY);

    this.renderSubtree(oCtx, this.treeLayout.root, this.treeLayout.totalFreq);

    try {
      const dataUrl = offscreen.toDataURL('image/png');
      const a = document.createElement('a');
      a.download = `huffman_tree_${this.currentFilename.replace(/[^a-zA-Z0-9_-]/g, '_')}.png`;
      a.href = dataUrl;
      document.body.appendChild(a);
      a.click();
      document.body.removeChild(a);
      App.showToast('Huffman tree diagram exported as PNG image!', 'success');
    } catch (e) {
      App.showToast('Failed to export PNG: ' + e.message, 'error');
    }
  },

  exportSVG() {
    if (!this.treeLayout || !this.treeLayout.root) {
      App.showToast('No tree data available to export.', 'error');
      return;
    }

    const padding = 50;
    const treeW = Math.max(200, this.treeLayout.maxX - this.treeLayout.minX + this.BOX_W + 40);
    const treeH = Math.max(200, this.treeLayout.maxY + 50);
    const width = Math.round(treeW + padding * 2);
    const height = Math.round(treeH + padding * 2);

    const offsetX = padding - this.treeLayout.minX + 20;
    const offsetY = padding + 30;

    let svgLines = [];
    let svgNodes = [];

    const traverse = (ln) => {
      if (!ln) return;
      const x = ln.x + offsetX;
      const y = ln.y + offsetY;
      const HALF_W = this.BOX_W / 2;
      const HALF_H = this.BOX_H / 2;

      if (ln.left) {
        const lx = ln.left.x + offsetX;
        const ly = ln.left.y + offsetY;
        svgLines.push(`<line x1="${x}" y1="${y + HALF_H}" x2="${lx}" y2="${ly - HALF_H}" stroke="#1a1a1a" stroke-width="1.5" />`);
        traverse(ln.left);
      }
      if (ln.right) {
        const rx = ln.right.x + offsetX;
        const ry = ln.right.y + offsetY;
        svgLines.push(`<line x1="${x}" y1="${y + HALF_H}" x2="${rx}" y2="${ry - HALF_H}" stroke="#1a1a1a" stroke-width="1.5" />`);
        traverse(ln.right);
      }

      let probStr = '0.0000';
      if (ln.isRoot) probStr = '1.0000';
      else if (this.treeLayout.totalFreq > 0) probStr = (ln.node.freq / this.treeLayout.totalFreq).toFixed(4);

      const botText = ln.isRoot ? 'ROOT' : (ln.bit || '0');

      let nodeSvg = `
        <g>
          <rect x="${x - HALF_W}" y="${y - HALF_H}" width="${this.BOX_W}" height="${this.BOX_H}" fill="#FFFFFF" stroke="#1a1a1a" stroke-width="1.5" />
          <line x1="${x - HALF_W}" y1="${y}" x2="${x + HALF_W}" y2="${y}" stroke="#1a1a1a" stroke-width="1.5" />
          <text x="${x}" y="${y - 4}" font-family="monospace" font-size="10" text-anchor="middle" fill="#1a1a1a">${probStr}</text>
          <text x="${x}" y="${y + 12}" font-family="${ln.isRoot ? 'sans-serif' : 'monospace'}" font-size="${ln.isRoot ? '9' : '12'}" font-weight="bold" text-anchor="middle" fill="#1a1a1a">${botText}</text>
      `;

      if (ln.isLeaf) {
        const stemLength = 8;
        const circleRadius = 12;
        const circleY = y + HALF_H + stemLength + circleRadius;
        const glyph = this.formatSymbolGlyph(ln.node).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');

        nodeSvg += `
          <line x1="${x}" y1="${y + HALF_H}" x2="${x}" y2="${y + HALF_H + stemLength}" stroke="#1a1a1a" stroke-width="1.5" />
          <circle cx="${x}" cy="${circleY}" r="${circleRadius}" fill="#FFFFFF" stroke="#1a1a1a" stroke-width="1.5" />
          <text x="${x}" y="${circleY + 4}" font-family="sans-serif" font-size="11" font-weight="bold" text-anchor="middle" fill="#1a1a1a">${glyph}</text>
        `;
      }

      nodeSvg += `</g>`;
      svgNodes.push(nodeSvg);
    };

    traverse(this.treeLayout.root);

    const svgContent = `<?xml version="1.0" standalone="no"?>
<svg xmlns="http://www.w3.org/2000/svg" width="${width}" height="${height}" viewBox="0 0 ${width} ${height}">
  <rect width="100%" height="100%" fill="#FFFFFF" />
  <text x="${padding}" y="28" font-family="sans-serif" font-size="14" font-weight="bold" fill="#202A35">HUFFMAN LAB - Tree Diagram: ${this.currentFilename}</text>
  <text x="${padding}" y="44" font-family="sans-serif" font-size="11" fill="#65717D">Total Bytes: ${this.treeLayout.totalFreq} | Unique Symbols: ${this.treeLayout.numLeaves}</text>
  ${svgLines.join('\n')}
  ${svgNodes.join('\n')}
</svg>`;

    const blob = new Blob([svgContent], { type: 'image/svg+xml;charset=utf-8' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.download = `huffman_tree_${this.currentFilename.replace(/[^a-zA-Z0-9_-]/g, '_')}.svg`;
    a.href = url;
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    URL.revokeObjectURL(url);
    App.showToast('Huffman tree diagram exported as SVG!', 'success');
  },

  renderCodebook(codes) {
    const tbody = document.getElementById('codebook-body');
    if (!tbody) return;
    tbody.innerHTML = '';

    if (!codes || Object.keys(codes).length === 0) {
      tbody.innerHTML = `<tr><td colspan="5" style="text-align:center; padding:1.5rem;">No codes generated yet.</td></tr>`;
      return;
    }

    const list = Object.entries(codes).map(([sym, info]) => ({
      sym: parseInt(sym, 10),
      ...info
    }));

    list.sort((a, b) => a.length - b.length || a.sym - b.sym);

    list.forEach(item => {
      const tr = document.createElement('tr');
      const hex = '0x' + item.sym.toString(16).toUpperCase().padStart(2, '0');
      const totalBits = (item.freq * item.length).toLocaleString();

      tr.innerHTML = `
        <td><strong class="code-pill">${item.label}</strong></td>
        <td class="mono">${hex}</td>
        <td class="mono">${item.freq.toLocaleString()}</td>
        <td><span class="mono" style="font-weight:700; color:var(--terracotta);">${item.code}</span></td>
        <td class="mono">${item.length} bits (${totalBits} total)</td>
      `;
      tbody.appendChild(tr);
    });
  }
};
