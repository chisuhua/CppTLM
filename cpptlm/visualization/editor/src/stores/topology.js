import { writable, derived } from 'svelte/store';

export const topology = writable({
  nodes: [],
  connections: []
});

export const selectedNode = writable(null);

export const nodeTypes = [
  { type: 'CacheTLM', color: '#90EE90', label: 'Cache' },
  { type: 'CrossbarTLM', color: '#FFA07A', label: 'Crossbar' },
  { type: 'MemoryTLM', color: '#DDA0DD', label: 'Memory' },
  { type: 'RouterTLM', color: '#FFA07A', label: 'Router' },
  { type: 'TrafficGenTLM', color: '#87CEEB', label: 'TrafficGen' }
];

export function addNode(type, x, y) {
  const id = `node_${Date.now()}`;
  topology.update(t => ({
    ...t,
    nodes: [...t.nodes, { id, type, x, y, config: {} }]
  }));
  return id;
}

export function updateNode(id, updates) {
  topology.update(t => ({
    ...t,
    nodes: t.nodes.map(n => n.id === id ? { ...n, ...updates } : n)
  }));
}

export function deleteNode(id) {
  topology.update(t => ({
    nodes: t.nodes.filter(n => n.id !== id),
    connections: t.connections.filter(c => c.source !== id && c.target !== id)
  }));
}

export function addConnection(source, target) {
  const id = `conn_${Date.now()}`;
  topology.update(t => ({
    ...t,
    connections: [...t.connections, { id, source, target }]
  }));
  return id;
}