import createModule from './example.js';

const phaseName = (phase) => {
  switch (phase) {
    case -1:
      return 'error';
    case 0:
      return 'idle';
    case 1:
      return 'fetching';
    case 2:
      return 'finishing';
    case 3:
      return 'done';
    default:
      return `unknown(${phase})`;
  }
};

const sleep = (ms) => new Promise((resolve) => setTimeout(resolve, ms));

const logNode = document.getElementById('log');
const statusNode = document.getElementById('status');
const runButton = document.getElementById('run');

const appendLog = (line) => {
  logNode.textContent += `${line}\n`;
  logNode.scrollTop = logNode.scrollHeight;
};

const setStatus = (line) => {
  statusNode.textContent = line;
};

const module = await createModule();
appendLog('Module initialized.');

const monitorPhase = () => {
  const phase = module._example_phase();
  const status = module._example_last_status();
  const size = module._example_last_size();
  setStatus(`phase=${phaseName(phase)} status=${status} size=${size}`);
};

const currentTrace = () => module.UTF8ToString(module._example_trace());

monitorPhase();

const runExample = async () => {
  runButton.disabled = true;
  logNode.textContent = '';
  module._example_reset();
  monitorPhase();

  const url = `./sample.txt?ts=${Date.now()}`;
  appendLog(`Starting fetch for ${url}`);

  let heartbeatCount = 0;
  const heartbeat = setInterval(() => {
    heartbeatCount += 1;
    const phase = module._example_phase();
    appendLog(`js heartbeat ${heartbeatCount}: phase=${phaseName(phase)}`);
    monitorPhase();
  }, 150);

  const urlPtr = module.stringToNewUTF8(url);
  const startedAt = performance.now();
  let callResult;

  try {
    callResult = module._example_run_fetch(urlPtr);
  } finally {
    module._free(urlPtr);
  }

  appendLog(`Immediate JS result type: ${callResult && typeof callResult.then === 'function' ? 'Promise' : typeof callResult}`);
  appendLog(`Phase immediately after call: ${phaseName(module._example_phase())}`);
  appendLog(`Trace immediately after call: ${currentTrace()}`);

  try {
    if (callResult && typeof callResult.then === 'function') {
      const fetchStatus = await callResult;
      appendLog(`JSPI export resolved with status=${fetchStatus} after ${(performance.now() - startedAt).toFixed(1)}ms`);
      appendLog(`Trace after Promise resolve: ${currentTrace()}`);
    } else {
      appendLog('Polling path active; waiting for wasm op completion from JS.');
      while (true) {
        const pollResult = module._example_poll_fetch();
        appendLog(`Trace after poll tick: ${currentTrace()}`);
        if (pollResult === 1) {
          appendLog(`Poll completed after ${(performance.now() - startedAt).toFixed(1)}ms`);
          break;
        }
        if (pollResult < 0) {
          appendLog(`Poll failed with code=${pollResult}`);
          break;
        }
        await sleep(100);
      }
    }
  } finally {
    clearInterval(heartbeat);
    monitorPhase();
    appendLog(`Final phase: ${phaseName(module._example_phase())}`);
    appendLog(`Final status: ${module._example_last_status()}`);
    appendLog(`Final size: ${module._example_last_size()} bytes`);
    appendLog(`Final trace: ${currentTrace()}`);
    runButton.disabled = false;
  }
};

runButton.addEventListener('click', () => {
  runExample().catch((error) => {
    appendLog(`Run failed: ${error}`);
    runButton.disabled = false;
  });
});

runExample().catch((error) => {
  appendLog(`Initial run failed: ${error}`);
  runButton.disabled = false;
});
