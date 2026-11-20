/**
 * Huffman Lab - Main Application Coordinator
 */
const App = {
  selectedFile: null,
  currentAnalysis: null,
  currentCompressionResult: null,
  recentHistory: [],

  init() {
    this.checkProtocol();
    this.setupTabs();
    this.setupUpload();
    this.setupSampleButtons();
    this.setupDecompressTab();
    this.loadHistory();
    TreeVisualizer.init();
    this.checkServerHealth();
  },

  checkProtocol() {
    if (window.location.protocol === 'file:') {
      const banner = document.getElementById('protocol-warning-banner');
      if (banner) banner.style.display = 'block';
      this.showToast('Notice: Please open http://localhost:8080/ in Chrome to communicate with C backend', 'info');
    }
  },

  formatError(err) {
    if (!err) return 'An unexpected error occurred.';
    let msg = err.message || err.toString();
    if (msg.includes('Failed to fetch') || msg.includes('NetworkError') || msg.includes('Load failed')) {
      if (window.location.protocol === 'file:') {
        return 'Chrome blocks API requests from file:///. Please open http://localhost:8080/ in your browser.';
      }
      return 'Cannot reach C backend server. Please ensure .\\huffman_server.exe 8080 is running.';
    }
    return msg;
  },

  async checkServerHealth() {
    try {
      await API.request('/api/health', { method: 'GET' });
    } catch (e) {
      // Backend may be starting
    }
  },

  setupTabs() {
    const tabs = document.querySelectorAll('.tab-btn');
    tabs.forEach(tab => {
      tab.addEventListener('click', () => {
        const targetView = tab.getAttribute('data-target');
        this.switchTab(targetView);
      });
    });
  },

  switchTab(viewId) {
    document.querySelectorAll('.tab-btn').forEach(btn => {
      btn.classList.toggle('active', btn.getAttribute('data-target') === viewId);
    });

    document.querySelectorAll('.view-section').forEach(sec => {
      sec.classList.toggle('active', sec.id === `view-${viewId}`);
    });

    if (viewId === 'visualizer') {
      requestAnimationFrame(() => {
        TreeVisualizer.resizeCanvas();
        TreeVisualizer.fitView();
      });
      setTimeout(() => {
        TreeVisualizer.resizeCanvas();
        TreeVisualizer.fitView();
      }, 100);
    }
  },

  setupUpload() {
    const dropzone = document.getElementById('main-dropzone');
    const fileInput = document.getElementById('main-file-input');

    if (dropzone && fileInput) {
      dropzone.addEventListener('click', () => fileInput.click());

      dropzone.addEventListener('dragover', (e) => {
        e.preventDefault();
        dropzone.classList.add('dragover');
      });

      dropzone.addEventListener('dragleave', () => {
        dropzone.classList.remove('dragover');
      });

      dropzone.addEventListener('drop', (e) => {
        e.preventDefault();
        dropzone.classList.remove('dragover');
        if (e.dataTransfer.files.length > 0) {
          this.handleFileSelected(e.dataTransfer.files[0]);
        }
      });

      fileInput.addEventListener('change', (e) => {
        if (e.target.files.length > 0) {
          this.handleFileSelected(e.target.files[0]);
        }
      });
    }

    document.getElementById('btn-action-analyze')?.addEventListener('click', () => {
      this.runAnalysis();
    });

    document.getElementById('btn-action-compress')?.addEventListener('click', () => {
      this.runCompression();
    });
  },

  setupSampleButtons() {
    document.querySelectorAll('.sample-btn').forEach(btn => {
      btn.addEventListener('click', () => {
        const type = btn.getAttribute('data-sample');
        this.loadSample(type);
      });
    });
  },

  loadSample(type) {
    let content = '';
    let filename = '';

    if (type === 'banana') {
      content = 'BANANA';
      filename = 'banana.txt';
    } else if (type === 'repeated') {
      content = 'A'.repeat(2048);
      filename = 'repeated_A.txt';
    } else if (type === 'text') {
      content = 'Huffman coding is an entropy encoding algorithm used for lossless data compression. The algorithm was developed by David A. Huffman while he was a Sc.D. student at MIT, and published in the 1952 paper "A Method for the Construction of Minimum-Redundancy Codes".';
      filename = 'huffman_overview.txt';
    } else if (type === 'single') {
      content = 'Z';
      filename = 'single_byte.txt';
    }

    const blob = new Blob([content], { type: 'text/plain' });
    const file = new File([blob], filename);
    this.handleFileSelected(file);
    this.showToast(`Loaded sample: ${filename}`, 'success');
  },

  handleFileSelected(file) {
    this.selectedFile = file;
    const badge = document.getElementById('selected-file-badge');
    const nameEl = document.getElementById('selected-file-name');
    const sizeEl = document.getElementById('selected-file-size');

    if (badge && nameEl && sizeEl) {
      badge.style.display = 'inline-flex';
      nameEl.textContent = file.name;
      sizeEl.textContent = `(${AnalysisModule.formatBytes(file.size)})`;
    }

    document.getElementById('btn-action-analyze').disabled = false;
    document.getElementById('btn-action-compress').disabled = false;
  },

  async runAnalysis() {
    if (!this.selectedFile) return;

    this.showToast('Analyzing file with C backend...', 'info');
    try {
      const data = await API.analyzeFile(this.selectedFile);
      this.currentAnalysis = data;
      AnalysisModule.render(data);

      // Load tree
      const treeRes = await API.getTree(data.id);
      TreeVisualizer.loadTree(treeRes.tree, this.selectedFile ? this.selectedFile.name : 'file');

      this.switchTab('analysis');
      this.showToast('Frequency analysis complete!', 'success');
    } catch (err) {
      this.showToast(this.formatError(err), 'error');
    }
  },

  async runCompression() {
    if (!this.selectedFile) return;

    this.showToast('Compressing file with C backend...', 'info');
    try {
      const result = await API.compress(this.selectedFile);
      this.currentCompressionResult = result;

      // Update tree visualizer with backend tree
      const treeRes = await API.getTree(result.id);
      TreeVisualizer.loadTree(treeRes.tree, this.selectedFile ? this.selectedFile.name : 'file');

      // Update analysis tab as well
      const analysisData = await API.analyzeFile(this.selectedFile);
      AnalysisModule.render(analysisData);

      this.renderResults(result);
      this.addToHistory(result);
      this.switchTab('results');
      this.showToast('Compression completed successfully!', 'success');
    } catch (err) {
      this.showToast(this.formatError(err), 'error');
    }
  },

  renderResults(res) {
    document.getElementById('res-orig-file').textContent = res.original_filename;
    document.getElementById('res-comp-file').textContent = res.compressed_filename;

    document.getElementById('res-orig-size').textContent = AnalysisModule.formatBytes(res.original_size);
    document.getElementById('res-comp-size').textContent = AnalysisModule.formatBytes(res.compressed_size);

    const ratio = (res.compression_ratio * 100).toFixed(2);
    document.getElementById('res-ratio').textContent = `${ratio}%`;

    const savedPct = res.space_saved_percent.toFixed(2);
    document.getElementById('res-saved-pct').textContent = `${savedPct}%`;

    const savedBytes = res.space_saved_bytes.toLocaleString();
    document.getElementById('res-saved-bytes').textContent = `${savedBytes} bytes`;

    document.getElementById('res-hash').textContent = res.original_hash || 'Computed';

    // Comparison bars
    const maxVal = Math.max(res.original_size, res.compressed_size) || 1;
    const origPct = Math.round((res.original_size / maxVal) * 100);
    const compPct = Math.round((res.compressed_size / maxVal) * 100);

    const origBar = document.getElementById('bar-orig-fill');
    const compBar = document.getElementById('bar-comp-fill');
    if (origBar) {
      origBar.style.width = `${origPct}%`;
      origBar.textContent = AnalysisModule.formatBytes(res.original_size);
    }
    if (compBar) {
      compBar.style.width = `${compPct}%`;
      compBar.textContent = AnalysisModule.formatBytes(res.compressed_size);
    }

    // Setup download button
    const dlBtn = document.getElementById('btn-download-compressed');
    if (dlBtn) {
      dlBtn.onclick = () => {
        window.location.href = API.getDownloadUrl(res.id, 'compressed');
      };
    }
  },

  setupDecompressTab() {
    const dropzone = document.getElementById('decompress-dropzone');
    const fileInput = document.getElementById('decompress-file-input');

    if (dropzone && fileInput) {
      dropzone.addEventListener('click', () => fileInput.click());

      dropzone.addEventListener('dragover', (e) => {
        e.preventDefault();
        dropzone.classList.add('dragover');
      });

      dropzone.addEventListener('dragleave', () => {
        dropzone.classList.remove('dragover');
      });

      dropzone.addEventListener('drop', (e) => {
        e.preventDefault();
        dropzone.classList.remove('dragover');
        if (e.dataTransfer.files.length > 0) {
          this.handleDecompressFile(e.dataTransfer.files[0]);
        }
      });

      fileInput.addEventListener('change', (e) => {
        if (e.target.files.length > 0) {
          this.handleDecompressFile(e.target.files[0]);
        }
      });
    }
  },

  async handleDecompressFile(file) {
    if (!file.name.endsWith('.huff')) {
      this.showToast('Notice: Standard compressed files use the .huff extension.', 'info');
    }

    this.showToast('Decompressing file with C backend...', 'info');
    try {
      const res = await API.decompressFile(file);

      const resBox = document.getElementById('decompress-result-box');
      if (resBox) resBox.style.display = 'block';

      document.getElementById('dec-input-name').textContent = res.compressed_filename;
      document.getElementById('dec-output-name').textContent = res.recovered_filename;
      document.getElementById('dec-input-size').textContent = AnalysisModule.formatBytes(res.compressed_size);
      document.getElementById('dec-output-size').textContent = AnalysisModule.formatBytes(res.recovered_size);
      document.getElementById('dec-hash').textContent = res.hash;

      const dlBtn = document.getElementById('btn-download-recovered');
      if (dlBtn) {
        dlBtn.onclick = () => {
          window.location.href = API.getDownloadUrl(res.id, 'decompressed');
        };
      }

      this.showToast('File decompressed with 100% fidelity!', 'success');
    } catch (err) {
      this.showToast(`Decompression failed: ${this.formatError(err)}`, 'error');
    }
  },

  addToHistory(item) {
    this.recentHistory.unshift({
      id: item.id,
      name: item.original_filename,
      orig_size: item.original_size,
      comp_size: item.compressed_size,
      saved: item.space_saved_percent,
      time: new Date().toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' })
    });

    if (this.recentHistory.length > 8) {
      this.recentHistory.pop();
    }

    try {
      localStorage.setItem('huffman_lab_history', JSON.stringify(this.recentHistory));
    } catch (e) {}

    this.renderHistory();
  },

  loadHistory() {
    try {
      const stored = localStorage.getItem('huffman_lab_history');
      if (stored) {
        this.recentHistory = JSON.parse(stored);
      }
    } catch (e) {}
    this.renderHistory();
  },

  renderHistory() {
    const container = document.getElementById('recent-history-list');
    if (!container) return;

    if (this.recentHistory.length === 0) {
      container.innerHTML = `<div style="text-align:center; padding:1.5rem; color:var(--steel-grey); font-size:0.85rem;">No recent files processed yet.</div>`;
      return;
    }

    container.innerHTML = '';
    this.recentHistory.forEach(item => {
      const div = document.createElement('div');
      div.className = 'history-item';
      div.innerHTML = `
        <div class="history-info">
          <span class="history-name">${item.name}</span>
          <span class="history-meta">${AnalysisModule.formatBytes(item.orig_size)} → ${AnalysisModule.formatBytes(item.comp_size)} (${item.saved.toFixed(1)}% saved) • ${item.time}</span>
        </div>
        <div>
          <button class="btn btn-secondary btn-sm" onclick="window.location.href='${API.getDownloadUrl(item.id, 'compressed')}'">Download .huff</button>
        </div>
      `;
      container.appendChild(div);
    });
  },

  showToast(message, type = 'info') {
    const container = document.getElementById('toast-container');
    if (!container) return;

    const toast = document.createElement('div');
    toast.className = `toast ${type}`;
    toast.textContent = message;
    container.appendChild(toast);

    setTimeout(() => {
      toast.style.opacity = '0';
      toast.style.transition = 'opacity 0.3s ease';
      setTimeout(() => toast.remove(), 300);
    }, 3500);
  }
};

window.addEventListener('DOMContentLoaded', () => {
  App.init();
});
