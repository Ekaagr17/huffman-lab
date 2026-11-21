const API = {
  baseUrl: (window.location.protocol === 'file:' || (window.location.port !== '8080' && window.location.port !== ''))
    ? 'http://localhost:8080'
    : '',

  async request(endpoint, options = {}) {
    const urls = [];
    if (this.baseUrl) {
      urls.push(`${this.baseUrl}${endpoint}`);
      urls.push(`http://127.0.0.1:8080${endpoint}`);
    } else {
      urls.push(endpoint);
      urls.push(`http://localhost:8080${endpoint}`);
      urls.push(`http://127.0.0.1:8080${endpoint}`);
    }

    let lastError = null;
    for (const url of urls) {
      try {
        const res = await fetch(url, options);
        return res;
      } catch (err) {
        lastError = err;
      }
    }
    throw lastError;
  },

  async analyzeFile(file, filename) {
    const formData = new FormData();
    formData.append('file', file, filename || file.name || 'unnamed.bin');

    const res = await this.request('/api/analyze', {
      method: 'POST',
      body: formData
    });

    const data = await res.json();
    if (!res.ok || !data.success) {
      throw new Error(data.error || 'Failed to analyze file.');
    }
    return data;
  },

  async compress(input) {
    let res;
    if (typeof input === 'string') {
      res = await this.request('/api/compress', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ id: input })
      });
    } else {
      const formData = new FormData();
      formData.append('file', input, input.name || 'unnamed.bin');
      res = await this.request('/api/compress', {
        method: 'POST',
        body: formData
      });
    }

    const data = await res.json();
    if (!res.ok || !data.success) {
      throw new Error(data.error || 'Compression failed.');
    }
    return data;
  },

  async decompressFile(file) {
    const formData = new FormData();
    formData.append('file', file, file.name);

    const res = await this.request('/api/decompress', {
      method: 'POST',
      body: formData
    });

    const data = await res.json();
    if (!res.ok || !data.success) {
      throw new Error(data.error || 'Decompression failed.');
    }
    return data;
  },

  async getTree(id) {
    const res = await this.request(`/api/tree/${id}`);
    const data = await res.json();
    if (!res.ok || !data.success) {
      throw new Error(data.error || 'Failed to fetch tree data.');
    }
    return data;
  },

  async getResult(id) {
    const res = await this.request(`/api/result/${id}`);
    const data = await res.json();
    if (!res.ok || !data.success) {
      throw new Error(data.error || 'Failed to fetch result.');
    }
    return data;
  },

  getDownloadUrl(id, type = 'compressed') {
    const base = this.baseUrl || '';
    return `${base}/api/download/${id}?type=${encodeURIComponent(type)}`;
  }
};
