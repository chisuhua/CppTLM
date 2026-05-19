<script>
  import Canvas from './components/Canvas.svelte';
  import Palette from './components/Palette.svelte';
  import PropertiesPanel from './components/PropertiesPanel.svelte';
  import { topology } from './stores/topology.js';

  function exportTopology() {
    const data = JSON.stringify($topology, null, 2);
    const blob = new Blob([data], { type: 'application/json' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = 'topology.json';
    a.click();
    URL.revokeObjectURL(url);
  }

  function importTopology() {
    const input = document.createElement('input');
    input.type = 'file';
    input.accept = '.json';
    input.onchange = async (e) => {
      const file = e.target.files[0];
      if (!file) return;
      const text = await file.text();
      try {
        const data = JSON.parse(text);
        if (data.nodes && Array.isArray(data.nodes)) {
          topology.set(data);
        } else {
          alert('Invalid topology file: missing nodes array');
        }
      } catch (err) {
        alert('Failed to parse topology file: ' + err.message);
      }
    };
    input.click();
  }
</script>

<main>
  <header>
    <h1>CppTLM Topology Editor</h1>
    <div class="header-buttons">
      <button on:click={importTopology}>Import JSON</button>
      <button on:click={exportTopology}>Export JSON</button>
    </div>
  </header>

  <div class="editor">
    <aside class="left-panel">
      <Palette />
      <PropertiesPanel />
    </aside>
    <section class="canvas-area">
      <Canvas />
    </section>
  </div>
</main>

<style>
  main { padding: 0; margin: 0; }
  header {
    display: flex;
    justify-content: space-between;
    align-items: center;
    padding: 10px 20px;
    background: #333;
    color: white;
  }
  header h1 { margin: 0; font-size: 18px; }
  .header-buttons { display: flex; gap: 10px; }
  .header-buttons button { padding: 8px 16px; cursor: pointer; }
  .editor { display: flex; height: calc(100vh - 60px); }
  .left-panel { width: 250px; display: flex; flex-direction: column; gap: 10px; padding: 10px; }
  .canvas-area { flex: 1; padding: 10px; }
</style>