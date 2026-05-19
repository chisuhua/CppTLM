<script>
  import { selectedNode, updateNode } from '../stores/topology.js';

  let localConfig = {};

  $: if ($selectedNode) {
    localConfig = { ...$selectedNode.config };
  }

  function handleChange() {
    if ($selectedNode) {
      updateNode($selectedNode.id, { config: localConfig });
    }
  }
</script>

<div class="properties">
  {#if $selectedNode}
    <h3>Properties: {$selectedNode.type}</h3>
    <div class="form-group">
      <label for="node-id">ID</label>
      <input id="node-id" type="text" value={$selectedNode.id} readonly>
    </div>
    <div class="form-group">
      <label for="node-type">Type</label>
      <input id="node-type" type="text" value={$selectedNode.type} readonly>
    </div>
  {:else}
    <p>Select a node to edit properties</p>
  {/if}
</div>

<style>
  .properties {
    border: 1px solid #ddd;
    padding: 10px;
    background: white;
    min-width: 200px;
  }
  .form-group { margin: 10px 0; }
  label { display: block; font-size: 12px; color: #666; }
  input { width: 100%; padding: 5px; box-sizing: border-box; }
</style>