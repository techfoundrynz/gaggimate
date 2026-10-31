import { useContext, useEffect, useRef, useState } from 'preact/hooks';
import { Trans, useLingui } from '@lingui/react/macro';
import { ApiServiceContext, machine } from '../../../services/ApiService.js';
import { downloadBlob } from '../../../utils/download.js';
import Section from '../../../components/Card.jsx';

const MAX_ENTRIES = 1000;
const ansi = new RegExp(`${String.fromCharCode(27)}\\[[0-9;]*m`, 'g');
const cleanText = text => text.replace(ansi, '').trimEnd();
const levelOf = text =>
  text
    .match(/\[([EWIDV])\]|^([EWIDV])\s*\(/)
    ?.slice(1)
    .find(Boolean) || 'I';

export function ConsoleSection() {
  const { t } = useLingui();
  const apiService = useContext(ApiServiceContext);
  const connected = machine.value.connected;
  const [streaming, setStreaming] = useState(false);
  const [visible, setVisible] = useState(!document.hidden);
  const [paused, setPaused] = useState(false);
  const [source, setSource] = useState('both');
  const [level, setLevel] = useState('all');
  const [entries, setEntries] = useState([]);
  const [controllerConnected, setControllerConnected] = useState(null);
  const output = useRef(null);
  const nextId = useRef(0);
  const observing = streaming && visible && connected;

  useEffect(() => {
    const onVisibility = () => setVisible(!document.hidden);
    document.addEventListener('visibilitychange', onVisibility);
    return () => document.removeEventListener('visibilitychange', onVisibility);
  }, []);

  useEffect(() => {
    if (!observing) return;
    setControllerConnected(null);
    const sequences = {};
    const listener = apiService.on('evt:console', message => {
      setControllerConnected(message.controllerConnected);
      const incoming = (message.entries || []).map(entry => {
        const previous = sequences[entry.source];
        const gap =
          previous !== undefined && entry.sequence > previous ? entry.sequence - previous - 1 : 0;
        sequences[entry.source] = entry.sequence;
        const text = cleanText(entry.text || '');
        return {
          ...entry,
          text,
          level: levelOf(text),
          lost: Math.max(gap, entry.dropped || 0),
          id: nextId.current++,
        };
      });
      if (incoming.length) setEntries(current => [...current, ...incoming].slice(-MAX_ENTRIES));
    });
    const subscribe = enabled => {
      if (apiService.socket?.readyState === WebSocket.OPEN) {
        apiService.send({ tp: 'req:console:subscribe', enabled });
      }
    };
    subscribe(true);
    const timer = setInterval(() => subscribe(true), 5000);
    const onPageHide = () => subscribe(false);
    window.addEventListener('pagehide', onPageHide);
    return () => {
      clearInterval(timer);
      window.removeEventListener('pagehide', onPageHide);
      subscribe(false);
      apiService.off('evt:console', listener);
    };
  }, [apiService, observing]);

  useEffect(() => {
    if (!paused && output.current) output.current.scrollTop = output.current.scrollHeight;
  }, [entries, paused, source, level]);

  const filtered = entries.filter(
    entry =>
      (source === 'both' || entry.source === source) && (level === 'all' || entry.level === level),
  );
  const download = () => {
    const text = filtered
      .map(
        entry =>
          `[${entry.source}] ${(entry.timestamp / 1000).toFixed(3)}s ${entry.lost ? `[${entry.lost} dropped] ` : ''}${entry.text}`,
      )
      .join('\n');
    downloadBlob(new Blob([text], { type: 'text/plain;charset=utf-8' }), 'gaggimate-console.log');
  };

  return (
    <Section title={t`Live Console`} className='md:col-span-2'>
      <div className='flex flex-wrap items-center justify-between gap-3'>
        <label className='flex cursor-pointer items-center gap-2 font-medium'>
          <input
            type='checkbox'
            className='toggle toggle-primary'
            checked={streaming}
            onChange={event => setStreaming(event.target.checked)}
          />
          <Trans>Stream device logs</Trans>
        </label>
        <span className='text-base-content/60 text-sm'>
          {!connected
            ? t`Disconnected`
            : observing
              ? t`Streaming`
              : streaming
                ? t`Paused while tab is hidden`
                : t`Stopped`}
          {observing && controllerConnected === false && ` · ${t`Controller disconnected`}`}
        </span>
      </div>
      <p className='text-base-content/60 text-sm'>
        <Trans>
          Shows logs from both devices. Streaming stops when you leave this screen or hide this tab.
        </Trans>
      </p>
      <div className='flex flex-wrap items-center gap-2'>
        <select
          className='select select-bordered select-sm'
          aria-label={t`Log source`}
          value={source}
          onChange={event => setSource(event.target.value)}
        >
          <option value='both'>{t`Both devices`}</option>
          <option value='display'>{t`Display`}</option>
          <option value='controller'>{t`Controller`}</option>
        </select>
        <select
          className='select select-bordered select-sm'
          aria-label={t`Log level`}
          value={level}
          onChange={event => setLevel(event.target.value)}
        >
          <option value='all'>{t`All levels`}</option>
          <option value='I'>INFO</option>
          <option value='W'>WARN</option>
          <option value='E'>ERROR</option>
        </select>
        <button
          type='button'
          className='btn btn-sm btn-ghost'
          onClick={() => setPaused(value => !value)}
        >
          {paused ? t`Resume scrolling` : t`Pause scrolling`}
        </button>
        <button type='button' className='btn btn-sm btn-ghost' onClick={() => setEntries([])}>
          <Trans>Clear</Trans>
        </button>
        <button
          type='button'
          className='btn btn-sm btn-ghost'
          disabled={!filtered.length}
          onClick={download}
        >
          <Trans>Download logs</Trans>
        </button>
      </div>
      <div
        ref={output}
        tabIndex={0}
        aria-label={t`Device console`}
        className='bg-base-300 h-80 overflow-auto rounded p-3 font-mono text-xs'
      >
        {!filtered.length && (
          <p className='text-base-content/50'>
            {observing ? t`Waiting for device logs…` : t`Enable streaming to view device logs.`}
          </p>
        )}
        {filtered.map(entry => (
          <div key={entry.id} className='mb-1 break-words whitespace-pre-wrap'>
            {entry.lost > 0 && (
              <div className='text-warning'>
                <Trans>{entry.lost} log records dropped before this entry.</Trans>
              </div>
            )}
            <span className={entry.source === 'controller' ? 'text-secondary' : 'text-primary'}>
              [{entry.source === 'controller' ? t`Controller` : t`Display`}]
            </span>{' '}
            <span className='text-base-content/50'>{(entry.timestamp / 1000).toFixed(3)}s </span>
            <span
              className={
                entry.level === 'E' ? 'text-error' : entry.level === 'W' ? 'text-warning' : ''
              }
            >
              {entry.text}
            </span>
          </div>
        ))}
      </div>
      <p className='text-base-content/50 text-xs'>
        <Trans>
          Latest 1,000 entries. Times are uptime on each device. Pause scrolling keeps streaming
          active.
        </Trans>
      </p>
    </Section>
  );
}
