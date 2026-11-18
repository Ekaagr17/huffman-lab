/**
 * Huffman Lab API Client
 * Clean interface to the C CivetWeb backend
 */
const API = {
  baseUrl: '',

  /**
   * Upload and analyze a file's byte frequencies
   * @param {File|Blob} file 
   * @param {string} [filename]
   */
  async analyzeFile(file, filename) {
    const formData = new FormData();
    formData.append('file', file, filename || file.name || 'unnamed.bin');

    const res = await fetch(`${this.baseUrl}/api/analyze`, {
      method: 'POST',
      body: formData
    });

    const data = await res.json();
    if (!res.ok || !data.success) {
      throw new Error(data.error || 'Failed to analyze file.');
    }
    return data;
  },

  /**
   * Compress file (either by existing analyzed ID or by fresh upload)
   * @param {string|File} input 
   */
  async compress(input) {
    let res;
    if (typeof input === 'string') {
      res = await fetch(`${this.baseUrl}/api/compress`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ id: input })
      });
    } else {
      const formData = new FormData();
      formData.append('file', input, input.name || 'unnamed.bin');
      res = await fetch(`${this.baseUrl}/api/compress`, {
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

  /**
   * Upload .huff file and decompress
   * @param {File} file 
   */
  async decompressFile(file) {
    const formData = new FormData();
    formData.append('file', file, file.name);

    const res = await fetch(`${this.baseUrl}/api/decompress`, {
      method: 'POST',
      body: formData
    });

    const data = await res.json();
    if (!res.ok || !data.success) {
      throw new Error(data.error || 'Decompression failed.');
    }
    return data;
  },

  /**
   * Fetch tree structure and construction steps
   * @param {string} id 
   */
  async getTree(id) {
    const res = await fetch(`${this.baseUrl}/api/tree/${id}`);
    const data = await res.json();
    if (!res.ok || !data.success) {
      throw new Error(data.error || 'Failed to fetch tree data.');
    }
    return data;
  },

  /**
   * Fetch complete result stats
   * @param {string} id 
   */
  async getResult(id) {
    const res = await fetch(`${this.baseUrl}/api/result/${id}`);
    const data = await res.json();
    if (!res.ok || !data.success) {
      throw new Error(data.error || 'Failed to fetch result.');
    }
    return data;
  },

  /**
   * Build download URL
   * @param {string} id 
   * @param {'compressed'|'decompressed'} type 
   */
  getDownloadUrl(id, type = 'compressed') {
    return `${this.baseUrl}/api/download/${id}?type=${type}`;
  }
};
