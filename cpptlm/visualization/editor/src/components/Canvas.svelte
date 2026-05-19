<script>
  import { topology, selectedNode, addNode, updateNode } from '../stores/topology.js';

  let dragging = null;
  let offset = { x: 0, y: 0 };

  function handleMouseDown(e, node) {
    dragging = node;
    offset = { x: e.clientX - node.x, y: e.clientY - node.y };
    selectedNode.set(node);
  }

  function handleMouseMove(e) {
    if (dragging) {
      updateNode(dragging.id, {
        x: e.clientX - offset.x,
        y: e.clientY - offset.y
      });
    }
  }

  function handleMouseUp() {
    dragging = null;
  }

  function handleDrop(e) {
    e.preventDefault();
    const type = e.dataTransfer.getData('nodeType');
    if (type) {
      const rect = e.currentTarget.getBoundingClientRect();
      addNode(type, e.clientX - rect.left, e.clientY - rect.top);
    }
  }

  function handleDragOver(e) {
    e.preventDefault();
  }
</script>

<!-- svelte-ignore a11y_no_noninteractive_element_interactions -->
<div class="canvas"
     role="application"
     aria-label="Topology canvas"
     on:mousemove={handleMouseMove}
     on:mouseup={handleMouseUp}
     on:drop={handleDrop}
     on:dragover={handleDragOver}>
  {#each $topology.nodes as node (node.id)}
    <div class="node"
         role="button"
         tabindex="0"
         style="left: {node.x}px; top: {node.y}px; background-color: {node.color || '#90EE90'}"
         on:mousedown={(e) => handleMouseDown(e, node)}>
      {node.type}
    </div>
  {/each}
</div>

<style>
  .canvas {
    position: relative;
    border: 1px solid #ccc;
    min-height: 400px;
    background: #fafafa;
  }
  .node {
    position: absolute;
    padding: 10px 15px;
    border-radius: 4px;
    cursor: move;
    user-select: none;
    box-shadow: 0 2px 4px rgba(0,0,0,0.1);
  }
</style>