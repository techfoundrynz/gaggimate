import { useEffect, useState } from 'preact/hooks';
import Section from './Card.jsx';

export default function HardwareScaleCalibration() {
  const [status, setStatus] = useState(null);
  const [mass, setMass] = useState(100);
  const [simMass, setSimMass] = useState(100);
  const [error, setError] = useState('');
  const [busy, setBusy] = useState(false);

  useEffect(() => {
    let cancelled = false;
    let timer;
    const poll = async () => {
      try {
        const response = await fetch('/api/scales/hardware');
        if (!response.ok) throw new Error('Unable to read wired scale status');
        const data = await response.json();
        if (!cancelled) setStatus(data);
      } catch (e) {
        if (!cancelled) {
          setStatus(null);
          setError(e.message);
        }
      } finally {
        if (!cancelled) timer = setTimeout(poll, 1000);
      }
    };
    poll();
    return () => {
      cancelled = true;
      clearTimeout(timer);
    };
  }, []);

  const command = async (action, extra = {}) => {
    setBusy(true);
    setError('');
    try {
      const response = await fetch('/api/scales/hardware', {
        method: 'POST',
        body: new URLSearchParams({ action, mass: String(mass), ...extra }),
      });
      const data = await response.json();
      setStatus(data);
      if (!response.ok) throw new Error(data.error || 'Scale command failed');
    } catch (e) {
      setError(e.message);
    } finally {
      setBusy(false);
    }
  };

  return (
    <Section title='Wired tray scales (dual HX711)'>
      <div className='space-y-4'>
        <p>
          Two load cells supporting one tray. When enabled, wired scales replace Bluetooth as the
          weight source.
        </p>
        {error && (
          <div className='alert alert-error' role='alert'>
            {error}
          </div>
        )}
        {!status && <p>Waiting for scale status…</p>}
        {status && (
          <>
            {!status.enabled && (
              <p>
                Enable Hardware Scales under Plugins, configure the GPIOs, then save and restart.
              </p>
            )}
            {status.enabled && (
              <>
                {status.simulated && (
                  <div className='rounded-box border-info space-y-3 border p-4'>
                    <p className='font-semibold'>Simulator load</p>
                    <p>
                      Place a virtual weight before each calibration capture, then wait two seconds.
                      Use the same mass as “Known weight”. Clear the tray before capturing empty.
                    </p>
                    <label className='form-control block'>
                      <span>Simulated weight (grams)</span>
                      <input
                        className='input input-bordered w-full'
                        type='number'
                        min='0'
                        max='500'
                        step='0.1'
                        value={simMass}
                        onInput={e => setSimMass(e.currentTarget.value)}
                      />
                    </label>
                    <div className='flex flex-wrap gap-2'>
                      {['left', 'centre', 'right'].map(position => (
                        <button
                          key={position}
                          type='button'
                          className='btn btn-outline'
                          disabled={busy}
                          onClick={() => command('simulate', { mass: String(simMass), position })}
                        >
                          Place {position}
                        </button>
                      ))}
                      <button
                        type='button'
                        className='btn btn-outline'
                        disabled={busy}
                        onClick={() => command('simulate', { mass: '0', position: 'centre' })}
                      >
                        Clear tray
                      </button>
                    </div>
                    <p>
                      Test load: {status.simMass} g at {status.simPosition}. Brewing and grinding
                      add weight automatically.
                    </p>
                  </div>
                )}
                <p>
                  Controller GPIOs: clock {status.clockPin}, left {status.leftPin}, right{' '}
                  {status.rightPin}.
                </p>
                {status.configError && (
                  <div className='alert alert-error'>
                    The controller rejected these GPIOs. Choose three distinct available pins in
                    Plugins; pins used by machine hardware, USB or flash cannot be used.
                  </div>
                )}
                <p>
                  {!status.available
                    ? 'Waiting for both HX711s. Check the controller firmware and wiring.'
                    : !status.calibrated
                      ? 'Connected — calibration required.'
                      : 'Connected and calibrated.'}
                </p>
                <p className='text-2xl font-semibold'>
                  {status.weight == null ? '—' : `${status.weight.toFixed(1)} g`}
                </p>
                <p>
                  Keep the tray installed. Use the same known weight for both positions; allow two
                  seconds to settle before each capture. Keep each 750 g cell within its rating,
                  including the tray.
                </p>
                {status.stage === 0 && (
                  <div className='flex flex-wrap gap-2'>
                    <button
                      type='button'
                      className='btn btn-primary'
                      disabled={busy || !status.samplesReady}
                      onClick={() => command('empty')}
                    >
                      1. Capture empty tray
                    </button>
                    <button
                      type='button'
                      className='btn btn-outline'
                      disabled={busy || !status.available || !status.calibrated}
                      onClick={() => command('tare')}
                    >
                      Tare
                    </button>
                  </div>
                )}
                {status.stage === 1 && (
                  <>
                    <label className='form-control block'>
                      <span>Known weight (grams)</span>
                      <input
                        className='input input-bordered w-full'
                        type='number'
                        min='10'
                        max='500'
                        step='0.1'
                        value={mass}
                        onInput={e => setMass(e.currentTarget.value)}
                      />
                    </label>
                    <p>Place the known weight near the left support.</p>
                    <button
                      type='button'
                      className='btn btn-primary'
                      disabled={busy || !status.samplesReady}
                      onClick={() => command('left')}
                    >
                      2. Capture left position
                    </button>
                  </>
                )}
                {status.stage === 2 && (
                  <>
                    <p>Move that same {status.mass} g weight near the right support.</p>
                    <button
                      type='button'
                      className='btn btn-primary'
                      disabled={busy || !status.samplesReady}
                      onClick={() => command('right')}
                    >
                      3. Capture right and save
                    </button>
                  </>
                )}
                {status.stage !== 0 && (
                  <button
                    type='button'
                    className='btn btn-ghost'
                    disabled={busy}
                    onClick={() => command('cancel')}
                  >
                    Cancel calibration
                  </button>
                )}
                <p className='text-sm'>
                  Calibration saves immediately. After calibration, check the known weight at the
                  left, centre and right of the tray. Brewing and grinding tare automatically.
                </p>
              </>
            )}
          </>
        )}
      </div>
    </Section>
  );
}
