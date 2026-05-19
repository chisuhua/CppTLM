<script>
  import { nodeTypes } from '../stores/topology.js';

  function handleDragStart(e, type) {
    e.dataTransfer.setData('nodeType', type);
  }
</script>

<div class="palette">
  <h3>Modules</h3>
  {#each nodeTypes as { type, color, label }}
    <div class="palette-item"
         role="listitem"
         draggable="true"
         on:dragstart={(e) => handleDragStart(e, type)}
         style="background-color: {color}">
      {label}
    </div>
  {/each}
</div>

<style>
  .palette {
    border: 1px solid #ddd;
    padding: 10px;
    background: white;
  }
  h3 { margin: 0 0 10px 0; font-size: 14px; }
  .palette-item {
    padding: 8px 12px;
    margin: 5px 0;
    border-radius: 4px;
    cursor: grab;
    font-size: 13px;
  }
  .palette-item:active { cursor: grabbing; }
</style>