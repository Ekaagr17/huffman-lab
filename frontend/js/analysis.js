/**
 * Frequency Analysis Module
 * Renders file metrics, frequency table, and distribution chart
 */
const AnalysisModule = {
  currentData: null,

  /**
   * Render analysis view with data from C backend
   * @param {Object} data 
   */
  render(data) {
    this.currentData = data;

    // File Summary Info
    document.getElementById('analysis-filename').textContent = data.filename || 'Unknown';
    document.getElementById('analysis-filesize').textContent = this.formatBytes(data.original_size);
    document.getElementById('analysis-bytes').textContent = data.original_size.toLocaleString();
    document.getElementById('analysis-unique').textContent = `${data.unique_symbols} / 256`;

    // Compute Shannon Entropy
    const entropy = this.calculateEntropy(data.frequencies, data.original_size);
    document.getElementById('analysis-entropy').textContent = `${entropy.toFixed(3)} bits/byte`;

    // Render Table
    this.renderTable(data.frequencies, data.original_size);

    // Render Chart
    this.renderChart(data.frequencies, data.original_size);
  },

  calculateEntropy(frequencies, totalBytes) {
    if (!frequencies || totalBytes === 0) return 0;
    let entropy = 0;
    for (const item of frequencies) {
      if (item.count > 0) {
        const p = item.count / totalBytes;
        entropy -= p * Math.log2(p);
      }
    }
    return entropy;
  },

  renderTable(frequencies, totalBytes, filter = '') {
    const tbody = document.getElementById('freq-table-body');
    tbody.innerHTML = '';

    if (!frequencies || frequencies.length === 0) {
      tbody.innerHTML = `<tr><td colspan="7" style="text-align:center; padding: 2rem;">No frequency data available.</td></tr>`;
      return;
    }

    const filtered = frequencies.filter(item => {
      if (!filter) return true;
      const q = filter.toLowerCase();
      const hex = '0x' + item.symbol.toString(16).padStart(2, '0').toLowerCase();
      const dec = item.symbol.toString();
      const lbl = (item.label || '').toLowerCase();
      return hex.includes(q) || dec.includes(q) || lbl.includes(q);
    });

    filtered.forEach((item, index) => {
      const tr = document.createElement('tr');
      const hex = '0x' + item.symbol.toString(16).toUpperCase().padStart(2, '0');
      const pct = (item.percentage || 0).toFixed(2);

      tr.innerHTML = `
        <td class="mono" style="color: var(--steel-grey);">${index + 1}</td>
        <td><strong class="code-pill">${this.escapeHtml(item.label)}</strong></td>
        <td class="mono">${hex}</td>
        <td class="mono">${item.symbol}</td>
        <td class="mono" style="font-weight:600;">${item.count.toLocaleString()}</td>
        <td class="mono">${pct}%</td>
        <td>
          <div class="pct-bar-container">
            <div class="pct-bar-bg">
              <div class="pct-bar" style="width: ${Math.min(100, Math.max(1, pct))}%;"></div>
            </div>
          </div>
        </td>
      `;
      tbody.appendChild(tr);
    });
  },

  renderChart(frequencies, totalBytes) {
    const container = document.getElementById('freq-chart-container');
    container.innerHTML = '';

    if (!frequencies || frequencies.length === 0) {
      container.innerHTML = `<div style="text-align:center; padding-top:4rem; color:var(--steel-grey);">No data to plot.</div>`;
      return;
    }

    // Show top 32 frequent symbols
    const top = frequencies.slice(0, 32);
    const maxCount = top[0]?.count || 1;

    const width = container.clientWidth || 800;
    const height = 240;
    const padding = { top: 20, right: 20, bottom: 40, left: 50 };
    const chartW = width - padding.left - padding.right;
    const chartH = height - padding.top - padding.bottom;
    const barW = Math.max(8, (chartW / top.length) - 4);

    let svg = `<svg width="100%" height="100%" viewBox="0 0 ${width} ${height}" style="overflow:visible;">`;

    // Baseline axis
    svg += `<line x1="${padding.left}" y1="${height - padding.bottom}" x2="${width - padding.right}" y2="${height - padding.bottom}" stroke="var(--stone)" stroke-width="1" />`;

    // Bars
    top.forEach((item, i) => {
      const x = padding.left + i * (chartW / top.length) + 2;
      const h = (item.count / maxCount) * chartH;
      const y = height - padding.bottom - h;
      const label = item.symbol >= 33 && item.symbol <= 126 ? String.fromCharCode(item.symbol) : ('x' + item.symbol.toString(16).toUpperCase());

      svg += `
        <g class="chart-bar-group">
          <rect x="${x}" y="${y}" width="${barW}" height="${h}" fill="var(--terracotta)" rx="2">
            <title>${item.label}: ${item.count} (${item.percentage.toFixed(1)}%)</title>
          </rect>
          <text x="${x + barW / 2}" y="${height - padding.bottom + 15}" text-anchor="middle" font-size="10" font-family="var(--font-mono)" fill="var(--steel-grey)">${this.escapeHtml(label)}</text>
        </g>
      `;
    });

    svg += `</svg>`;
    container.innerHTML = svg;
  },

  formatBytes(bytes) {
    if (bytes === 0) return '0 B';
    const k = 1024;
    const sizes = ['B', 'KB', 'MB', 'GB'];
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i];
  },

  escapeHtml(str) {
    if (!str) return '';
    return str.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');
  }
};
