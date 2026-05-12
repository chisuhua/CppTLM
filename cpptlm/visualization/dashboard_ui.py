"""
cpptlm/visualization/dashboard_ui.py — HTML templates for DashboardServer.
Provides _DASHBOARD_HTML (legacy), _HOME_HTML (runs list), make_run_view_html() (per-run view).
"""

from __future__ import annotations

_DASHBOARD_HTML = r"""<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>CppTLM 实时 Dashboard</title>
<script src="https://cdn.plot.ly/plotly-2.35.2.min.js"></script>
<style>
  body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; margin: 0; padding: 20px; background: #f8f9fa; color: #333; }
  h1 { font-size: 1.5em; border-bottom: 2px solid #333; padding-bottom: 8px; }
  .status-bar { display: flex; gap: 20px; margin: 10px 0; padding: 10px; background: #e9ecef; border-radius: 6px; font-size: 0.9em; }
  .status-bar .label { color: #666; }
  .chart-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 16px; }
  .chart-box { background: white; border-radius: 8px; box-shadow: 0 1px 3px rgba(0,0,0,0.1); padding: 12px; }
  .chart-box h3 { margin: 0 0 8px 0; font-size: 1em; color: #555; }
  .chart-box .plotly-graph-div { width: 100%; height: 300px; }
  .error { color: #dc3545; padding: 10px; }
  .table-wrap { overflow-x: auto; }
  table { border-collapse: collapse; width: 100%; font-size: 0.85em; }
  th, td { border: 1px solid #dee2e6; padding: 6px 10px; text-align: left; }
  th { background: #f1f3f5; }
  td.num { text-align: right; font-variant-numeric: tabular-nums; }
  @media (max-width: 900px) { .chart-grid { grid-template-columns: 1fr; } }
</style>
</head>
<body>
<h1>CppTLM 仿真实时 Dashboard</h1>
<div class="status-bar" id="status">
  <span><span class="label">Cycle:</span> <span id="cycle">—</span></span>
  <span><span class="label">Records:</span> <span id="records">0</span></span>
  <span><span class="label">Groups:</span> <span id="groups">—</span></span>
  <span><span class="label">Last update:</span> <span id="update">—</span></span>
</div>
<div class="chart-grid" id="charts"></div>

<script>
const POLL_MS = 2000;
let lastData = null;

async function poll() {
  try {
    const resp = await fetch('/data');
    const data = await resp.json();
    lastData = data;
    render(data);
    document.getElementById('update').textContent = new Date().toLocaleTimeString();
  } catch(e) {
    console.warn('poll error:', e);
  }
  setTimeout(poll, POLL_MS);
}

function render(data) {
  document.getElementById('cycle').textContent = data.simulation_cycle || '—';
  document.getElementById('records').textContent = data.record_count || 0;
  document.getElementById('groups').textContent = (data.groups || []).join(', ') || '—';

  const container = document.getElementById('charts');
  container.innerHTML = '';

  function createChartBox(title) {
    const box = document.createElement('div');
    box.className = 'chart-box';
    box.innerHTML = '<h3>' + title + '</h3><div class="plot"></div>';
    return box;
  }

  for (const group of (data.groups || [])) {
    const groupData = data.by_group[group];
    if (!groupData) continue;

    if (groupData.latency_cycle && groupData.latency_cycle.length > 0) {
      const box = createChartBox(group + ' 延迟 (周期)');
      const trace = {
        x: groupData.latency_cycle, y: groupData.latency_avg,
        type: 'scatter', mode: 'lines+markers',
        name: 'avg', line: { color: '#2196F3' },
      };
      Plotly.newPlot(box.querySelector('.plot'), [trace], {
        margin: { t: 10, r: 10, b: 30, l: 50 },
        xaxis: { title: 'Cycle' },
        yaxis: { title: 'Latency (cycle)' },
      });
      container.appendChild(box);
    }

    if (groupData.req_cycle && groupData.req_cycle.length > 0) {
      const box = createChartBox(group + ' 请求统计');
      const traces = [];
      if (groupData.hits) traces.push({ x: groupData.req_cycle, y: groupData.hits, type: 'scatter', mode: 'lines+markers', name: 'hits', line: { color: '#4CAF50' } });
      if (groupData.misses) traces.push({ x: groupData.req_cycle, y: groupData.misses, type: 'scatter', mode: 'lines+markers', name: 'misses', line: { color: '#f44336' } });
      Plotly.newPlot(box.querySelector('.plot'), traces, {
        margin: { t: 10, r: 10, b: 30, l: 50 },
        xaxis: { title: 'Cycle' },
        yaxis: { title: 'Count' },
      });
      container.appendChild(box);
    }
  }

  if (data.metrics && data.metrics.length > 0) {
    const box = document.createElement('div');
    box.className = 'chart-box';
    box.innerHTML = '<h3>性能摘要</h3><div class="table-wrap"><table><tr><th>Group</th><th>Mean</th><th>P95</th><th>P99</th><th>Max</th><th>Count</th></tr>' +
      data.metrics.map(m => '<tr><td>' + m.group + '</td><td class="num">' + m.mean.toFixed(2) + '</td><td class="num">' + m.p95.toFixed(2) + '</td><td class="num">' + m.p99.toFixed(2) + '</td><td class="num">' + m.max.toFixed(2) + '</td><td class="num">' + m.count + '</td></tr>').join('') +
      '</table></div>';
    container.appendChild(box);
  }

  if (data.bottlenecks && data.bottlenecks.length > 0) {
    const box = document.createElement('div');
    box.className = 'chart-box';
    box.innerHTML = '<h3>瓶颈检测</h3><div class="table-wrap"><table><tr><th>Group</th><th>问题</th><th>建议</th></tr>' +
      data.bottlenecks.map(b => '<tr><td>' + b.group + '</td><td>' + b.problem + '</td><td>' + b.suggestion + '</td></tr>').join('') +
      '</table></div>';
    container.appendChild(box);
  }
}

poll();
</script>
</body>
</html>"""

_HOME_HTML = r"""<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="utf-8">
<title>CppTLM Unified Dashboard</title>
<script src="https://cdn.plot.ly/plotly-2.35.2.min.js"></script>
<style>
body { font-family: -apple-system, sans-serif; margin: 20px; background: #f8f9fa; }
h1 { border-bottom: 2px solid #333; }
.run-card { background: white; border-radius: 8px; padding: 16px; margin: 12px 0;
            box-shadow: 0 1px 3px rgba(0,0,0,0.1); }
.run-card.active { border-left: 4px solid #28a745; }
.run-card.done { border-left: 4px solid #6c757d; }
.status-dot { display: inline-block; width: 10px; height: 10px; border-radius: 50%; background: #28a745; }
.status-dot.done { background: #6c757d; }
.btn { padding: 6px 12px; border: none; border-radius: 4px; cursor: pointer; }
.btn-primary { background: #007bff; color: white; }
.btn-secondary { background: #6c757d; color: white; }
</style>
</head>
<body>
<h1>CppTLM 统一 Dashboard</h1>
<p>端口: <span id="port"></span> | <a href="/">刷新</a></p>
<div id="runs-list">加载中...</div>
<script>
async function loadRuns() {
    try {
        const resp = await fetch('/api/runs');
        const runs = await resp.json();
        const container = document.getElementById('runs-list');
        document.getElementById('port').textContent = window.location.port;
        if (runs.length === 0) {
            container.innerHTML = '<p>暂无运行记录。请使用 <code>cpptlm run --config &lt;file.json&gt;</code> 开始仿真。</p>';
            return;
        }
        container.innerHTML = runs.map(function(run) {
            return '<div class="run-card ' + (run.is_active ? 'active' : 'done') + '">' +
                '<h3><span class="status-dot' + (run.is_active ? '' : ' done') + '"></span> ' + run.run_id + '</h3>' +
                '<p>创建时间: ' + run.created_at + '</p>' +
                '<p>Cycles: ' + (run.params && run.params.cycles ? run.params.cycles : '—') +
                ' | Status: ' + (run.is_active ? '● Running' : '✓ Completed') + '</p>' +
                '<a href="/?run=' + encodeURIComponent(run.run_id) + '" class="btn btn-primary">View</a>' +
                (run.is_active ? '' : ' <button class="btn btn-secondary" onclick="rerun(\'' + run.run_id + '\')">Re-run</button>') +
                '</div>';
        }).join('');
    } catch(e) {
        document.getElementById('runs-list').innerHTML = '<p class="error">加载失败: ' + e + '</p>';
    }
}
function rerun(runId) {
    window.location.href = '/?run=' + encodeURIComponent(runId) + '&rerun=1';
}
loadRuns();
</script>
</body>
</html>"""


def make_run_view_html(run_id: str, is_active: bool) -> str:
    active_js = "true" if is_active else "false"
    return r"""<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="utf-8">
<title>Run: """ + run_id + r"""</title>
<script src="https://cdn.plot.ly/plotly-2.35.2.min.js"></script>
<style>
body { font-family: -apple-system, sans-serif; margin: 20px; background: #f8f9fa; }
.nav { margin: 16px 0; }
.nav button { padding: 8px 16px; margin-right: 4px; border: none; border-radius: 4px; cursor: pointer; background: #e9ecef; }
.nav button.active { background: #007bff; color: white; }
.tab { display: none; background: white; padding: 16px; border-radius: 8px; }
.tab.active { display: block; }
.status { padding: 8px 12px; background: #e9ecef; border-radius: 4px; margin-bottom: 16px; }
.status.running { color: #28a745; }
.status.done { color: #6c757d; }
.btn { padding: 6px 12px; border: none; border-radius: 4px; cursor: pointer; }
.btn-primary { background: #007bff; color: white; }
.btn-secondary { background: #6c757d; color: white; }
.error { color: #dc3545; }
textarea { font-family: monospace; }
</style>
</head>
<body>
<h1>Run: <span id="run-id"></span> <span id="status-badge" class="status done"></span></h1>
<div class="nav">
    <button class="active" onclick="showTab('topology')">Topology</button>
    <button onclick="showTab('metrics')">Metrics</button>
    <button onclick="showTab('config')">Config</button>
    <button onclick="showTab('report')">Report</button>
</div>

<div id="tab-topology" class="tab active">
    <div id="topology-content">加载中...</div>
</div>

<div id="tab-metrics" class="tab">
    <div id="metrics-stats"></div>
    <div id="latency-chart"></div>
</div>

<div id="tab-config" class="tab">
    <div id="editor-toolbar">
        <button class="btn btn-primary" onclick="saveConfig()">Save</button>
        <span id="editor-status" style="margin-left:12px;color:#666;"></span>
        <span id="editor-fallback-warn" style="display:none;margin-left:12px;color:#dc3545;">
            (Monaco CDN 不可用，使用纯文本模式)
        </span>
    </div>
    <div id="monaco-container" style="width:100%;height:500px;border:1px solid #ccc;"></div>
    <textarea id="config-editor" style="width:100%;height:500px;font-family:monospace;display:none;"></textarea>
    <div style="margin-top:8px;">
        <button class="btn btn-secondary" onclick="showRerunForm()">Re-run</button>
    </div>
    <div id="rerun-form" style="display:none;margin-top:12px;">
        <label>Cycles: <input type="number" id="rerun-cycles" value="50000"/></label>
        <button class="btn btn-primary" onclick="doRerun()">Run</button>
    </div>
    <div id="config-message" style="margin-top:8px;"></div>
</div>

<div id="tab-report" class="tab">
    <div id="report-content">加载中...</div>
</div>

<script>
var RUN_ID = '""" + run_id + r"""';
var isActive = """ + active_js + r""";
var statsOffset = 0;

function showTab(name) {
    document.querySelectorAll('.tab').forEach(function(t) { t.classList.remove('active'); });
    document.querySelectorAll('.nav button').forEach(function(b) { b.classList.remove('active'); });
    document.getElementById('tab-' + name).classList.add('active');
    var idx = ['topology','metrics','config','report'].indexOf(name) + 1;
    document.querySelector('.nav button:nth-child(' + idx + ')').classList.add('active');
}

async function init() {
    try {
        var resp = await fetch('/api/runs/' + encodeURIComponent(RUN_ID));
        var data = await resp.json();
        document.getElementById('run-id').textContent = RUN_ID;

        var badge = document.getElementById('status-badge');
        if (data.is_active) {
            badge.textContent = '● Running';
            badge.className = 'status running';
        } else {
            badge.textContent = '✓ Completed';
            badge.className = 'status done';
        }

        if (data.has_topology) {
            document.getElementById('topology-content').innerHTML =
                '<img src="/runs/' + encodeURIComponent(RUN_ID) + '/topology.png" style="max-width:100%"/>';
        } else {
            document.getElementById('topology-content').innerHTML = '<p>无 topology 图片</p>';
        }

        try {
            var cfgResp = await fetch('/api/runs/' + encodeURIComponent(RUN_ID) + '/config');
            var cfgText = await cfgResp.text();
            if (useTextarea) {
                document.getElementById('config-editor').value = cfgText;
            } else {
                monacoEditor.setValue(cfgText);
            }
        } catch(e) {
            if (useTextarea) {
                document.getElementById('config-editor').value = '// 无法加载 config.json: ' + e;
            }
        }

        initMonaco();

        if (data.has_report) {
            document.getElementById('report-content').innerHTML =
                '<iframe src="/runs/' + encodeURIComponent(RUN_ID) + '/report.html" style="width:100%;height:600px;border:none;"></iframe>';
        } else {
            document.getElementById('report-content').innerHTML = '<p>无 report.html</p>';
        }

        if (data.is_active) {
            pollStats();
        } else {
            loadStaticMetrics();
        }
    } catch(e) {
        document.getElementById('run-id').textContent = RUN_ID + ' (加载失败)';
        console.error(e);
    }
}

async function pollStats() {
    if (!isActive) return;
    try {
        var resp = await fetch('/api/runs/' + encodeURIComponent(RUN_ID) + '/stats?offset=' + statsOffset);
        var result = await resp.json();
        statsOffset = result.offset;
        renderLiveStats(result.records);
    } catch(e) { console.warn('poll stats error:', e); }
    setTimeout(pollStats, 2000);
}

function renderLiveStats(records) {
    if (!records || records.length === 0) return;
    var last = records[records.length - 1];
    var lat = (last.data && last.data.latency) || {};
    document.getElementById('metrics-stats').innerHTML =
        '<p>最新 Cycle: <strong>' + (last.simulation_cycle || 0) + '</strong>' +
        (lat.avg !== undefined ? ' | 延迟 avg: <strong>' + lat.avg.toFixed(2) + '</strong>' : '') + '</p>';
}

async function loadStaticMetrics() {
    try {
        var resp = await fetch('/api/runs/' + encodeURIComponent(RUN_ID) + '/stats?offset=0');
        var result = await resp.json();
        var statsDiv = document.getElementById('metrics-stats');
        var chartDiv = document.getElementById('latency-chart');
        if (result.records && result.records.length > 0) {
            statsDiv.innerHTML = '<p>共 ' + result.records.length + ' 条统计记录（离线模式）</p>';
            var byGroup = {};
            result.records.forEach(function(r) {
                var g = r.group || '?';
                if (!byGroup[g]) byGroup[g] = {cycles: [], avgs: []};
                byGroup[g].cycles.push(r.simulation_cycle || 0);
                byGroup[g].avgs.push((r.data && r.data.latency && r.data.latency.avg) || 0);
            });
            var traces = Object.keys(byGroup).map(function(g) {
                return {x: byGroup[g].cycles, y: byGroup[g].avgs, type: 'scatter', mode: 'lines+markers', name: g, line: {color: '#2196F3'}};
            });
            Plotly.newPlot(chartDiv, traces, {margin: {t:10,r:10,b:30,l:50}, xaxis: {title:'Cycle'}, yaxis: {title:'Latency (cycle)'}});
        } else {
            statsDiv.innerHTML = '<p>暂无 metrics 数据</p>';
        }
    } catch(e) {
        document.getElementById('metrics-stats').innerHTML = '<p class="error">加载失败: ' + e + '</p>';
    }
}

async function saveConfig() {
    let configText;
    if (useTextarea) {
        configText = document.getElementById('config-editor').value;
    } else {
        configText = monacoEditor.getValue();
    }
    const status = document.getElementById('editor-status');
    status.textContent = '保存中...';
    status.style.color = '#666';
    try {
        var resp = await fetch('/api/runs/' + encodeURIComponent(RUN_ID) + '/config', {
            method: 'POST', headers: {'Content-Type': 'application/json'},
            body: JSON.stringify({config: configText})
        });
        var result = await resp.json();
        if (resp.ok) {
            status.textContent = '✓ 已保存';
            status.style.color = '#28a745';
            setTimeout(function() { status.textContent = ''; }, 3000);
        } else {
            throw new Error('Server error: ' + (result.error || resp.status));
        }
    } catch(e) {
        status.textContent = '✗ 保存失败: ' + e;
        status.style.color = '#dc3545';
    }
}

let monacoEditor = null;
let useTextarea = false;

async function initMonaco() {
    const container = document.getElementById('monaco-container');
    const textarea = document.getElementById('config-editor');
    const warn = document.getElementById('editor-fallback-warn');

    try {
        await new Promise((resolve, reject) => {
            const script = document.createElement('script');
            script.src = 'https://cdn.jsdelivr.net/npm/monaco-editor@0.45.0/min/vs/loader.js';
            script.onload = resolve;
            script.onerror = reject;
            document.head.appendChild(script);
        });

        require.config({
            paths: { vs: 'https://cdn.jsdelivr.net/npm/monaco-editor@0.45.0/min/vs' }
        });

        await new Promise((resolve, reject) => {
            require(['vs/editor/editor.main'], function () {
                monacoEditor = monaco.editor.create(container, {
                    value: '',
                    language: 'json',
                    theme: 'vs',
                    automaticLayout: true,
                    scrollBeyondLastLine: false,
                    minimap: { enabled: false },
                    folding: true,
                    lineNumbers: 'on',
                    renderWhitespace: 'selection',
                });
                resolve();
            }, reject);
        });

        try {
            const schemaResp = await fetch('/api/schema');
            if (schemaResp.ok) {
                const schema = await schemaResp.json();
                monaco.languages.json.jsonDefaults.setDiagnosticsOptions({
                    validate: true,
                    schemas: [{
                        uri: 'https://cpptlm/schema/config.json',
                        fileMatch: ['*'],
                        schema: schema
                    }]
                });
            }
        } catch (e) {
        }

    } catch (e) {
        useTextarea = true;
        container.style.display = 'none';
        textarea.style.display = 'block';
        warn.style.display = 'inline';
    }
}

function showRerunForm() {
    var form = document.getElementById('rerun-form');
    form.style.display = form.style.display === 'none' ? 'block' : 'none';
}

async function doRerun() {
    var cycles = parseInt(document.getElementById('rerun-cycles').value) || 50000;
    var msgDiv = document.getElementById('config-message');
    try {
        var resp = await fetch('/api/runs/' + encodeURIComponent(RUN_ID) + '/rerun', {
            method: 'POST', headers: {'Content-Type': 'application/json'},
            body: JSON.stringify({cycles: cycles})
        });
        var result = await resp.json();
        if (resp.ok) {
            msgDiv.textContent = '重新运行已启动 (PID: ' + result.pid + ')';
            setTimeout(function() { window.location.reload(); }, 3000);
        } else {
            msgDiv.textContent = '错误: ' + (result.error || resp.status);
            msgDiv.className = 'error';
        }
    } catch(e) {
        msgDiv.textContent = '错误: ' + e;
        msgDiv.className = 'error';
    }
}

init();
</script>
</body>
</html>"""


__all__ = ["_DASHBOARD_HTML", "_HOME_HTML", "make_run_view_html"]
