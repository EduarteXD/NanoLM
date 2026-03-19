#pragma once

static const char WEB_UI[] PROGMEM = R"HTML(
<!doctype html>
<html lang="zh-CN">
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover" />
  <title>Nano LM</title>
  <style>
    :root {
      --bg0: #08131f;
      --bg1: #12344d;
      --bg2: #1d5f63;
      --ink: #ecf8ff;
      --muted: #bdd4e6;
      --card: rgba(255,255,255,.11);
      --line: rgba(255,255,255,.22);
      --ok: #8bf5bd;
      --warn: #ffd479;
      --err: #ff8e8e;
      --btn0: #1a4a67;
      --btn1: #267289;
    }
    * { box-sizing: border-box; }
    body {
      margin: 0;
      min-height: 100vh;
      color: var(--ink);
      font-family: "Noto Sans SC", "Microsoft YaHei", "PingFang SC", sans-serif;
      background:
        radial-gradient(circle at 12% 14%, rgba(255,255,255,.15), transparent 28%),
        radial-gradient(circle at 84% 20%, rgba(255,255,255,.10), transparent 34%),
        linear-gradient(140deg, var(--bg0), var(--bg1) 45%, var(--bg2));
      padding: 16px;
    }
    .app {
      width: min(1100px, 100%);
      margin: 0 auto;
      border: 1px solid var(--line);
      border-radius: 18px;
      background: rgba(5,18,30,.62);
      backdrop-filter: blur(8px);
      box-shadow: 0 22px 56px rgba(0,0,0,.32);
      padding: 14px;
    }
    .head {
      display: flex;
      justify-content: space-between;
      align-items: center;
      gap: 10px;
      flex-wrap: wrap;
      margin-bottom: 12px;
    }
    h1 {
      margin: 0;
      font-size: clamp(22px, 3.8vw, 34px);
      letter-spacing: .5px;
      font-weight: 800;
    }
    .tag {
      font-size: 13px;
      padding: 7px 12px;
      border-radius: 999px;
      border: 1px solid var(--line);
      color: var(--muted);
      background: rgba(0,0,0,.26);
    }
    .grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(150px, 1fr));
      gap: 10px;
    }
    .card {
      border: 1px solid var(--line);
      border-radius: 14px;
      background: var(--card);
      padding: 12px;
      min-height: 92px;
    }
    .k {
      margin: 0 0 6px;
      color: var(--muted);
      font-size: 12px;
      letter-spacing: 1px;
      text-transform: uppercase;
    }
    .v {
      margin: 0;
      font-size: clamp(20px, 3vw, 28px);
      font-weight: 750;
    }
    .panel {
      margin-top: 12px;
      border: 1px solid var(--line);
      border-radius: 14px;
      background: var(--card);
      padding: 12px;
    }
    .status {
      margin-top: 6px;
      font-size: 14px;
      color: var(--muted);
    }
    .ok { color: var(--ok); }
    .warn { color: var(--warn); }
    .err { color: var(--err); }
    .form-grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(150px, 1fr));
      gap: 10px;
    }
    label {
      display: flex;
      flex-direction: column;
      gap: 5px;
      font-size: 12px;
      color: var(--muted);
      letter-spacing: .8px;
      text-transform: uppercase;
    }
    input, select, button {
      border: 1px solid var(--line);
      border-radius: 10px;
      background: rgba(0,0,0,.30);
      color: var(--ink);
      padding: 9px 10px;
      font-size: 14px;
      outline: none;
    }
    input:disabled { opacity: .5; }
    .row {
      margin-top: 10px;
      display: flex;
      flex-wrap: wrap;
      align-items: center;
      gap: 10px;
    }
    button {
      cursor: pointer;
      font-weight: 700;
      background: linear-gradient(130deg, var(--btn0), var(--btn1));
      border: 1px solid rgba(255,255,255,.25);
    }
    .hint {
      font-size: 12px;
      color: var(--muted);
    }
    .chart-box {
      margin-top: 8px;
      height: 220px;
      border: 1px solid var(--line);
      border-radius: 10px;
      background: rgba(0,0,0,.26);
      overflow: hidden;
    }
    #traceChart {
      width: 100%;
      height: 100%;
      display: block;
    }
  </style>
</head>
<body>
  <main class="app">
    <section class="head">
      <h1>Nano LM</h1>
      <span class="tag" id="netTag">网络连接中...</span>
    </section>

    <section class="grid">
      <article class="card"><p class="k">照度 Lux</p><p class="v" id="lux">--</p></article>
      <article class="card"><p class="k">EV100</p><p class="v" id="ev100">--</p></article>
      <article class="card"><p class="k">EV</p><p class="v" id="evIso">--</p></article>
      <article class="card"><p class="k">传感器状态</p><p class="v" id="sensorState">--</p></article>
    </section>

    <section class="panel">
      <p class="k">曝光建议</p>
      <p class="v" id="recommend">--</p>
      <p class="status" id="status">状态: --</p>
    </section>

    <section class="panel">
      <p class="k">测光曲线</p>
      <div class="chart-box">
        <canvas id="traceChart" width="980" height="220"></canvas>
      </div>
      <div class="row">
        <span class="hint" id="traceHint">实时曲线：青=原始Lux，橙=趋势Lux</span>
      </div>
    </section>

    <section class="panel">
      <p class="k" style="margin-bottom:10px;">测光设置</p>
      <div class="form-grid">
        <label>
          模式
          <select id="mode">
            <option value="aperture">光圈优先</option>
            <option value="shutter">快门优先</option>
          </select>
        </label>
        <label>
          ISO
          <input id="iso" type="number" min="25" max="25600" step="1" value="100" />
        </label>
        <label>
          光圈（f）
          <input id="aperture" type="number" min="0.7" max="32" step="0.1" value="2.8" />
        </label>
        <label>
          快门（秒 或 1/125）
          <input id="shutter" type="text" value="1/125" />
        </label>
        <label>
          曝光补偿（档）
          <input id="expComp" type="number" min="-5" max="5" step="0.1" value="0.0" />
        </label>
        <label>
          校准常数 C
          <input id="calibC" type="number" min="100" max="500" step="1" value="250" />
        </label>
      </div>
      <div class="row">
        <button id="saveBtn" type="button">保存设置</button>
        <span class="hint">光圈优先：固定光圈求快门；快门优先：固定快门求光圈。</span>
      </div>
    </section>
  </main>

  <script>
    const $ = (id) => document.getElementById(id);

    const ApiClient = {
      getData: () => fetch('/api/data', { cache: 'no-store' }).then(r => {
        if (!r.ok) throw new Error('data');
        return r.json();
      }),
      getConfig: () => fetch('/api/config', { cache: 'no-store' }).then(r => {
        if (!r.ok) throw new Error('config');
        return r.json();
      }),
      saveConfig: (payload) => {
        const q = new URLSearchParams(payload);
        return fetch('/api/config?' + q.toString(), { method: 'POST' }).then(r => {
          if (!r.ok) throw new Error('save');
          return r.json();
        });
      }
    };

    class StatusComponent {
      set(text, level) {
        const el = $('status');
        el.textContent = '状态: ' + text;
        el.className = 'status ' + (level || '');
      }
    }

    class MetricsComponent {
      static fmt(v, digits = 2) {
        if (v === null || v === undefined || Number.isNaN(Number(v))) return '--';
        return Number(v).toFixed(digits);
      }
      static stateText(state) {
        if (state === 'ok') return '正常';
        if (state === 'saturated') return '过曝';
        if (state === 'error') return '错误';
        return '等待';
      }
      render(data) {
        // $('full').textContent = data.full;
        // $('ir').textContent = data.ir;
        // $('visible').textContent = data.visible;
        // $('age').textContent = data.sample_age_ms + ' ms';
        $('lux').textContent = data.lux === null ? '--' : MetricsComponent.fmt(data.lux, 2);
        $('ev100').textContent = data.ev100 === null ? '--' : MetricsComponent.fmt(data.ev100, 2);
        $('evIso').textContent = data.ev_iso === null ? '--' : MetricsComponent.fmt(data.ev_iso, 2);
        $('sensorState').textContent =
          MetricsComponent.stateText(data.sensor_state) + ' ' +
          data.gain_x + 'x/' + data.integration_ms + 'ms';
        $('netTag').textContent = data.wifi_mode + ' @ ' + data.ip;
      }
    }

    class TraceChartComponent {
      constructor() {
        this.canvas = $('traceChart');
        this.ctx = this.canvas.getContext('2d');
        this.maxPoints = 180;
        this.rawLux = [];
        this.trendLux = [];
        this.dpr = window.devicePixelRatio || 1;
        this.resize();
        window.addEventListener('resize', () => this.resize());
      }

      reset() {
        this.rawLux.length = 0;
        this.trendLux.length = 0;
      }

      resize() {
        const rect = this.canvas.getBoundingClientRect();
        const w = Math.max(320, Math.floor(rect.width || 980));
        const h = Math.max(160, Math.floor(rect.height || 220));
        this.canvas.width = Math.floor(w * this.dpr);
        this.canvas.height = Math.floor(h * this.dpr);
        this.ctx.setTransform(this.dpr, 0, 0, this.dpr, 0, 0);
        this.draw();
      }

      pushPoint(data) {
        const raw = data.lux === null ? NaN : Number(data.lux);
        this.rawLux.push(raw);
        const lastTrend = this.trendLux.length ? this.trendLux[this.trendLux.length - 1] : NaN;
        const trend = Number.isFinite(raw)
          ? (Number.isFinite(lastTrend) ? (lastTrend * 0.8 + raw * 0.2) : raw)
          : lastTrend;
        this.trendLux.push(trend);
        while (this.rawLux.length > this.maxPoints) {
          this.rawLux.shift();
          this.trendLux.shift();
        }
      }

      toLog(v) {
        if (!Number.isFinite(v) || v <= 0) return NaN;
        return Math.log10(Math.max(0.1, v));
      }

      drawGrid(x, y, w, h) {
        const ctx = this.ctx;
        ctx.strokeStyle = 'rgba(255,255,255,0.14)';
        ctx.lineWidth = 1;
        ctx.setLineDash([]);
        for (let i = 0; i <= 4; i++) {
          const yy = y + (h * i / 4);
          ctx.beginPath();
          ctx.moveTo(x, yy);
          ctx.lineTo(x + w, yy);
          ctx.stroke();
        }
      }

      drawSeries(values, color, lineWidth, x, y, w, h, minLog, maxLog, dash) {
        const ctx = this.ctx;
        const n = values.length;
        if (n < 2) return;
        ctx.strokeStyle = color;
        ctx.lineWidth = lineWidth;
        ctx.setLineDash(dash || []);
        ctx.beginPath();
        let started = false;
        for (let i = 0; i < n; i++) {
          const lv = this.toLog(values[i]);
          if (!Number.isFinite(lv)) {
            started = false;
            continue;
          }
          const px = x + (w * i / Math.max(1, n - 1));
          const py = y + (maxLog - lv) * h / Math.max(0.0001, (maxLog - minLog));
          if (!started) {
            ctx.moveTo(px, py);
            started = true;
          } else {
            ctx.lineTo(px, py);
          }
        }
        ctx.stroke();
      }

      drawLegend(x, y) {
        const ctx = this.ctx;
        const items = [
          ['#7fe6ff', 'Raw Lux'],
          ['#ffbd72', 'Trend Lux']
        ];
        let offset = x;
        ctx.font = '12px "Noto Sans SC", "Microsoft YaHei", sans-serif';
        for (const item of items) {
          const color = item[0];
          const text = item[1];
          ctx.fillStyle = color;
          ctx.fillRect(offset, y - 8, 14, 3);
          ctx.fillStyle = 'rgba(236,248,255,0.9)';
          ctx.fillText(text, offset + 18, y);
          offset += 98;
        }
      }

      draw(data) {
        const ctx = this.ctx;
        const w = this.canvas.width / this.dpr;
        const h = this.canvas.height / this.dpr;
        ctx.clearRect(0, 0, w, h);

        const padL = 44;
        const padR = 12;
        const padT = 16;
        const padB = 26;
        const pw = Math.max(40, w - padL - padR);
        const ph = Math.max(40, h - padT - padB);

        this.drawGrid(padL, padT, pw, ph);

        const logs = [];
        for (const v of this.rawLux) { const lv = this.toLog(v); if (Number.isFinite(lv)) logs.push(lv); }
        for (const v of this.trendLux) { const lv = this.toLog(v); if (Number.isFinite(lv)) logs.push(lv); }

        if (!logs.length) {
          ctx.fillStyle = 'rgba(236,248,255,0.75)';
          ctx.font = '13px "Noto Sans SC", "Microsoft YaHei", sans-serif';
          ctx.fillText('等待采样数据...', padL, padT + 24);
          return;
        }

        let minLog = Math.min(...logs);
        let maxLog = Math.max(...logs);
        if ((maxLog - minLog) < 0.4) {
          maxLog += 0.2;
          minLog -= 0.2;
        }

        this.drawSeries(this.rawLux, '#7fe6ff', 1.6, padL, padT, pw, ph, minLog, maxLog, []);
        this.drawSeries(this.trendLux, '#ffbd72', 1.8, padL, padT, pw, ph, minLog, maxLog, []);

        ctx.strokeStyle = 'rgba(255,255,255,0.28)';
        ctx.lineWidth = 1;
        ctx.setLineDash([]);
        ctx.strokeRect(padL, padT, pw, ph);

        ctx.fillStyle = 'rgba(236,248,255,0.9)';
        ctx.font = '11px "Noto Sans SC", "Microsoft YaHei", sans-serif';
        ctx.fillText('Lux(log10)', 6, padT + 10);
        ctx.fillText('最近样本', w - 60, h - 8);
        this.drawLegend(padL + 4, padT + 14);

        const maxLux = Math.pow(10, maxLog);
        const minLux = Math.pow(10, minLog);
        $('traceHint').textContent =
          '实时曲线：青=原始Lux，橙=趋势Lux；范围约 ' +
          minLux.toFixed(2) + ' ~ ' + maxLux.toFixed(2) + ' Lux';
      }

      render(data) {
        this.pushPoint(data);
        this.draw(data);
      }
    }

    class RecommendComponent {
      render(data) {
        if (data.mode === 'aperture' && data.shutter_suggest_text) {
          $('recommend').textContent = '建议：' + data.shutter_suggest_text + ' @ f/' +
            Number(data.aperture_set).toFixed(1);
          return;
        }
        if (data.mode === 'shutter' && data.aperture_suggest_text) {
          $('recommend').textContent = '建议：f/' + Number(data.aperture_suggest).toFixed(1) +
            ' @ ' + data.shutter_set_text;
          return;
        }
        $('recommend').textContent = '--';
      }
    }

    class SettingsComponent {
      constructor() {
        $('mode').addEventListener('change', (e) => this.syncByMode(e.target.value));
      }
      syncByMode(mode) {
        $('aperture').disabled = mode !== 'aperture';
        $('shutter').disabled = mode !== 'shutter';
      }
      apply(cfg) {
        $('mode').value = cfg.mode;
        $('iso').value = cfg.iso;
        $('aperture').value = Number(cfg.aperture).toFixed(1);
        $('shutter').value = cfg.shutter_text;
        $('expComp').value = Number(cfg.exp_comp).toFixed(1);
        $('calibC').value = Number(cfg.calib_c).toFixed(0);
        this.syncByMode(cfg.mode);
      }
      collectPayload() {
        return {
          mode: $('mode').value,
          iso: $('iso').value,
          aperture: $('aperture').value,
          shutter: $('shutter').value,
          exp_comp: $('expComp').value,
          calib_c: $('calibC').value
        };
      }
    }

    class App {
      constructor() {
        this.status = new StatusComponent();
        this.metrics = new MetricsComponent();
        this.reco = new RecommendComponent();
        this.chart = new TraceChartComponent();
        this.settings = new SettingsComponent();
        this.loadedConfig = false;
        this.pendingTick = false;
        $('saveBtn').addEventListener('click', () => this.onSave());
      }

      async loadConfig() {
        const cfg = await ApiClient.getConfig();
        this.settings.apply(cfg);
        this.loadedConfig = true;
      }

      async onSave() {
        try {
          const cfg = await ApiClient.saveConfig(this.settings.collectPayload());
          this.settings.apply(cfg);
          this.status.set('设置已保存', 'ok');
        } catch (_) {
          this.status.set('保存失败', 'err');
        }
      }

      applyState(data) {
        this.metrics.render(data);
        this.reco.render(data);
        this.chart.render(data);

        if (data.sensor_state === 'ok') this.status.set('测光正常', 'ok');
        else if (data.sensor_state === 'saturated') this.status.set('传感器过曝（自动降档中）', 'warn');
        else if (data.sensor_state === 'error') this.status.set('传感器读取异常', 'err');
        else this.status.set('等待首帧采样', '');

        if (!this.loadedConfig) {
          this.settings.apply(data);
          this.loadedConfig = true;
        }
      }

      async tick() {
        if (this.pendingTick) return;
        this.pendingTick = true;
        try {
          const data = await ApiClient.getData();
          this.applyState(data);
        } catch (_) {
          this.status.set('网络/API 异常', 'err');
        } finally {
          this.pendingTick = false;
        }
      }

      async start() {
        try { await this.loadConfig(); } catch (_) {}
        await this.tick();
        setInterval(() => this.tick(), 300);
      }
    }

    new App().start();
  </script>
</body>
</html>
)HTML";
