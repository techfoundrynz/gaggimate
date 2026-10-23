import { faTrashCan } from '@fortawesome/free-solid-svg-icons/faTrashCan';
import { FontAwesomeIcon } from '@fortawesome/react-fontawesome';
import homekitImage from '../../assets/homekit.png';
import { faCalendarDays } from '@fortawesome/free-solid-svg-icons/faCalendarDays';
import { computed } from '@preact/signals';
import { machine, ApiServiceContext } from '../../services/ApiService.js';
import { useCallback, useContext, useEffect, useState } from 'preact/hooks';
import { Trans, useLingui } from '@lingui/react/macro';
import { useProfileList } from '../../services/hooks.js';
import { Fragment } from 'preact';

const gearpumpAddon = computed(() => machine.value.capabilities.gearpumpAddon);

const MAHLKONIG_MIN_RECIPES = 4;

// Profile per grinder recipe slot, stored as "r1,r2,..."; the grinder reports only the recipe number.
function MahlkonigRecipeProfiles({ formData, onChange }) {
  const { t } = useLingui();
  const apiService = useContext(ApiServiceContext);
  const [lastRecipe, setLastRecipe] = useState(null);
  const { profiles: allProfiles } = useProfileList();
  const profiles = allProfiles.filter(p => !p.utility);
  const mapped = String(formData.mahlkonigRecipeProfiles || '').split(',');
  const count = Math.max(MAHLKONIG_MIN_RECIPES, mapped.length, lastRecipe || 0);
  const recipes = Array.from({ length: count }, (_, i) => i + 1);

  useEffect(() => {
    apiService
      .request({ tp: 'req:mahlkonig:status' })
      .then(res => setLastRecipe(res.grind?.recipe || null))
      .catch(() => setLastRecipe(null));
  }, [apiService]);

  const setProfile = (recipe, value) => {
    const next = recipes.map(r => (r === recipe ? value : (mapped[r - 1] ?? '')));
    while (next.length > MAHLKONIG_MIN_RECIPES && !next[next.length - 1]) next.pop();
    onChange('mahlkonigRecipeProfiles')({ currentTarget: { value: next.join(',') } });
  };

  return (
    <div className='form-control'>
      <span className='mb-2 block text-sm font-medium'>
        <Trans>Profile per grinder recipe</Trans>
      </span>
      <div className='grid grid-cols-1 gap-4 sm:grid-cols-2'>
        {recipes.map(recipe => {
          const value = mapped[recipe - 1] ?? '';
          const missing = value && profiles.length > 0 && !profiles.some(p => p.id === value);
          return (
            <div key={recipe}>
              <label htmlFor={`mahlkonigRecipe${recipe}`} className='mb-1 block text-sm opacity-70'>
                <Trans>Recipe {recipe}</Trans>
              </label>
              <select
                id={`mahlkonigRecipe${recipe}`}
                className='select select-bordered w-full'
                value={value}
                onChange={e => setProfile(recipe, e.currentTarget.value)}
              >
                <option value=''>{t`Keep selected profile`}</option>
                {missing && <option value={value}>{t`Deleted profile`}</option>}
                {profiles.map(p => (
                  <option key={p.id} value={p.id}>
                    {p.label}
                  </option>
                ))}
              </select>
            </div>
          );
        })}
      </div>
      <p className='mt-2 text-sm opacity-70'>
        {lastRecipe
          ? t`Your grinder last ground recipe ${lastRecipe}.`
          : t`Grind once with GbS to see which recipe number your grinder reports.`}{' '}
        <Trans>The profile's dose is saved to the shot history.</Trans>
      </p>
    </div>
  );
}

const SETTE_STATUS_POLL_MS = 2000;

const SETTE_STATUS_LABELS = {
  preparing: 'Grinding',
  completed: 'Grind completed',
  cancelled: 'Grind cancelled',
  failed: 'Grind failed',
  ended: 'Grind ended',
};

const formatAgo = ms => {
  const s = Math.round(ms / 1000);
  if (s < 60) return `${s}s`;
  if (s < 3600) return `${Math.floor(s / 60)}m ${s % 60}s`;
  return `${Math.floor(s / 3600)}h ${Math.floor((s % 3600) / 60)}m`;
};

const SETTE_DEBUG_ACTIONS = [
  { action: 'reconnect', label: 'Reconnect now' },
  { action: 'resend-target', label: 'Resend target' },
  { action: 'simulate-grind', label: 'Simulate grind' },
  { action: 'clear-dose', label: 'Clear pending dose' },
  { action: 'clear-log', label: 'Clear log' },
];

// Link internals and an event log, polled only while the panel is open
function SetteDebug() {
  const apiService = useContext(ApiServiceContext);
  const [open, setOpen] = useState(false);
  const [debug, setDebug] = useState(null);
  const [copied, setCopied] = useState(false);

  const poll = useCallback(
    () =>
      apiService
        .request({ tp: 'req:sette:debug' })
        .then(setDebug)
        .catch(() => setDebug(null)),
    [apiService],
  );

  useEffect(() => {
    if (!open) return undefined;
    poll();
    const timer = setInterval(poll, SETTE_STATUS_POLL_MS);
    return () => clearInterval(timer);
  }, [open, poll]);

  const runAction = action =>
    apiService
      .request({ tp: 'req:sette:debug-action', action })
      .then(() => setTimeout(poll, 300))
      .catch(() => {});

  const logText = debug
    ? debug.log
        .map(e => `-${formatAgo(e.ago).padStart(7)}  ${e.text}`)
        .reverse()
        .join('\n')
    : '';

  const copy = () => {
    const report = `${JSON.stringify({ ...debug, log: undefined }, null, 2)}\n\n${logText}`;
    navigator.clipboard?.writeText(report).then(() => {
      setCopied(true);
      setTimeout(() => setCopied(false), 1500);
    });
  };

  const rows = debug && [
    ['Controller link', debug.controllerLinked ? 'up' : 'down (grinder link paused)'],
    ['BLE connections', `${debug.bleClients} / ${debug.bleMaxClients}`],
    [
      'Grinder link',
      debug.connected
        ? `connected ${formatAgo(debug.connectedFor)}, RSSI ${debug.rssi} dBm`
        : debug.nextConnectIn > 0
          ? `next attempt in ${formatAgo(debug.nextConnectIn)}`
          : 'connecting…',
    ],
    ['Characteristic', debug.characteristic || '—'],
    ['Connect attempts', `${debug.connectAttempts} (${debug.connectFailures} failed)`],
    ['Frames sent', `${debug.txFrames} (${debug.txFailures} failed)`],
    [
      'Frames received',
      `${debug.rxFrames} (${debug.rxChecksumErrors} bad checksum)${
        debug.lastRxAgo > 0 ? `, last ${formatAgo(debug.lastRxAgo)} ago` : ''
      }`,
    ],
    ['Live weight', `${debug.liveWeight.toFixed(1)} g${debug.cycleActive ? ' (grinding)' : ''}`],
    ['Target', `${debug.desiredTarget.toFixed(1)} g wanted, ${debug.sentTarget.toFixed(1)} g sent`],
    [
      'Pending dose',
      debug.pendingDose > 0
        ? `${debug.pendingDose.toFixed(1)} g, ${formatAgo(debug.pendingDoseAge)} old`
        : 'none',
    ],
    ['Display uptime', formatAgo(debug.uptime)],
  ];

  return (
    <details onToggle={e => setOpen(e.currentTarget.open)}>
      <summary className='cursor-pointer text-sm opacity-70'>Debug</summary>
      {open && !debug && <div className='mt-2 text-sm opacity-70'>Loading…</div>}
      {open && debug && (
        <div className='mt-2 space-y-3'>
          <div className='grid grid-cols-[auto_1fr] gap-x-4 gap-y-1 text-sm'>
            {rows.map(([label, value]) => (
              <Fragment key={label}>
                <span className='opacity-70'>{label}</span>
                <span className='font-mono break-all'>{value}</span>
              </Fragment>
            ))}
          </div>
          <div className='flex flex-wrap gap-2'>
            {SETTE_DEBUG_ACTIONS.map(({ action, label }) => (
              <button
                key={action}
                type='button'
                className='btn btn-xs btn-outline'
                onClick={() => runAction(action)}
              >
                {label}
              </button>
            ))}
            <button type='button' className='btn btn-xs btn-outline' onClick={copy}>
              {copied ? 'Copied' : 'Copy report'}
            </button>
          </div>
          <p className='text-xs opacity-70'>
            Simulate grind makes the next shot take the grind target as its dose, as if the grinder
            had just finished.
          </p>
          <pre className='bg-base-300 max-h-64 overflow-auto rounded p-2 text-xs'>
            {logText || 'No events yet.'}
          </pre>
        </div>
      )}
    </details>
  );
}

// The plugin finds and remembers the nearest grinder by itself; this shows the link and lets you pick another one.
function SetteGrinder() {
  const apiService = useContext(ApiServiceContext);
  const [status, setStatus] = useState(null);
  const [scanned, setScanned] = useState(false);
  const [manualAddress, setManualAddress] = useState('');
  const [manualError, setManualError] = useState(false);

  useEffect(() => {
    let cancelled = false;
    const poll = () =>
      apiService
        .request({ tp: 'req:sette:status' })
        .then(res => !cancelled && setStatus(res))
        .catch(() => !cancelled && setStatus(null));
    poll();
    const timer = setInterval(poll, SETTE_STATUS_POLL_MS);
    return () => {
      cancelled = true;
      clearInterval(timer);
    };
  }, [apiService]);

  const scan = () => {
    setScanned(true);
    setStatus(s => ({ ...s, scanning: true, found: [] }));
    apiService.request({ tp: 'req:sette:scan' }).catch(() => {});
  };
  const chooseAddress = address =>
    apiService
      .request({ tp: 'req:sette:set-address', address })
      .then(res => {
        if (!res.ok) {
          setManualError(true);
          return;
        }
        setManualError(false);
        setScanned(false);
        setManualAddress('');
        setStatus(s => ({ ...s, address, connected: false }));
      })
      .catch(() => {});

  if (!status) {
    return (
      <div className='text-sm opacity-70'>
        Save and restart to start the grinder link. GaggiMate then finds your grinder by itself:
        press a button on the grinder to wake it.
      </div>
    );
  }

  return (
    <>
      <div className='text-sm'>
        {status.address ? (
          <>
            <div>
              <span className='font-medium'>Grinder:</span>{' '}
              <span className='font-mono'>{status.address}</span>
              {' · '}
              {status.connected ? 'Connected' : 'Not connected'}
              {status.connected && status.target > 0 && `, target ${status.target.toFixed(1)} g`}
            </div>
            {!status.connected && (
              <div className='opacity-70'>
                Press a button on the grinder to wake it; GaggiMate reconnects within 20 seconds.
              </div>
            )}
          </>
        ) : (
          <div>
            <span className='font-medium'>Searching for your grinder…</span>{' '}
            <span className='opacity-70'>Press a button on the grinder to wake it.</span>
          </div>
        )}
        {status.lastStatus !== 'none' && (
          <div className='opacity-70'>
            <span className='font-medium'>Last grind:</span>{' '}
            {SETTE_STATUS_LABELS[status.lastStatus] || status.lastStatus}
            {status.lastGrindWeight > 0 && ` (${status.lastGrindWeight.toFixed(1)} g)`}
            {status.pendingDose > 0 && ', used as the next shot’s dose'}
          </div>
        )}
      </div>

      <div className='flex flex-wrap gap-2'>
        <button
          type='button'
          className='btn btn-sm btn-secondary'
          onClick={scan}
          disabled={status.scanning}
        >
          {status.scanning ? 'Scanning…' : 'Scan'}
        </button>
        {status.address && (
          <button
            type='button'
            className='btn btn-sm btn-outline'
            onClick={() => chooseAddress('')}
          >
            Forget
          </button>
        )}
      </div>

      {scanned && !status.scanning && (
        <div className='space-y-2'>
          {status.found?.length > 0 ? (
            <div className='flex flex-wrap gap-2'>
              {status.found.map(g => (
                <button
                  key={g.address}
                  type='button'
                  className='btn btn-sm btn-outline'
                  onClick={() => chooseAddress(g.address)}
                  disabled={g.address === status.address}
                >
                  {g.name} <span className='font-mono opacity-70'>{g.address}</span>
                  {g.address === status.address && ' (current)'}
                </button>
              ))}
            </div>
          ) : (
            <div className='text-sm opacity-70'>
              No grinders found. Press a button on the grinder to wake it, then scan again.
            </div>
          )}
        </div>
      )}

      <details>
        <summary className='cursor-pointer text-sm opacity-70'>Enter an address manually</summary>
        <div className='mt-2 flex gap-2'>
          <input
            type='text'
            aria-label='Grinder Bluetooth address'
            className={`input input-bordered input-sm w-full font-mono ${manualError ? 'input-error' : ''}`}
            placeholder='aa:bb:cc:dd:ee:ff'
            value={manualAddress}
            onInput={e => setManualAddress(e.currentTarget.value.trim())}
          />
          <button
            type='button'
            className='btn btn-sm'
            onClick={() => chooseAddress(manualAddress)}
            disabled={!manualAddress}
          >
            Use
          </button>
        </div>
      </details>

      <SetteDebug />
    </>
  );
}

export function PluginCard({
  formData,
  onChange,
  autowakeupSchedules,
  addAutoWakeupSchedule,
  removeAutoWakeupSchedule,
  updateAutoWakeupTime,
  updateAutoWakeupDay,
}) {
  const { t } = useLingui();
  const dayNames = [
    t`Monday`,
    t`Tuesday`,
    t`Wednesday`,
    t`Thursday`,
    t`Friday`,
    t`Saturday`,
    t`Sunday`,
  ];
  const dayLetters = [
    t({ message: 'M', context: 'Monday, one letter' }),
    t({ message: 'T', context: 'Tuesday, one letter' }),
    t({ message: 'W', context: 'Wednesday, one letter' }),
    t({ message: 'T', context: 'Thursday, one letter' }),
    t({ message: 'F', context: 'Friday, one letter' }),
    t({ message: 'S', context: 'Saturday, one letter' }),
    t({ message: 'S', context: 'Sunday, one letter' }),
  ];
  return (
    <div className='space-y-4'>
      <div className='bg-base-200 rounded-lg p-4'>
        <div className='flex items-center justify-between'>
          <span className='text-xl font-medium'>
            <Trans>Automatic Wakeup Schedule</Trans>
          </span>
          <input
            id='autowakeupEnabled'
            name='autowakeupEnabled'
            value='autowakeupEnabled'
            type='checkbox'
            className='toggle toggle-primary'
            checked={!!formData.autowakeupEnabled}
            onChange={onChange('autowakeupEnabled')}
            aria-label={t`Enable Auto Wakeup`}
          />
        </div>
        {formData.autowakeupEnabled && (
          <div className='border-base-300 mt-4 space-y-4 border-t pt-4'>
            <p className='text-sm opacity-70'>
              <Trans>Automatically switch to brew mode at specified time(s) of day.</Trans>
            </p>
            <div className='form-control'>
              <label className='mb-2 block text-sm font-medium'>
                <Trans>Auto Wakeup Schedule</Trans>
              </label>
              <div className='space-y-2'>
                {autowakeupSchedules?.map((schedule, scheduleIndex) => (
                  <div
                    key={scheduleIndex}
                    className='flex flex-wrap items-center gap-1 md:flex-nowrap'
                  >
                    {/* Time input */}
                    <div className='grow-1 text-center sm:text-start'>
                      <input
                        type='time'
                        className='input input-bordered input-sm md:input-md w-auto min-w-0 pr-6 text-center'
                        value={schedule.time}
                        onChange={e => updateAutoWakeupTime(scheduleIndex, e.target.value)}
                        disabled={!formData.autowakeupEnabled}
                      />
                    </div>

                    {/* Days toggle buttons */}
                    <div
                      className='join flex grow-8'
                      role='group'
                      aria-label={t`Days of week selection`}
                    >
                      {dayLetters.map((dayLabel, dayIndex) => (
                        <button
                          key={dayIndex}
                          type='button'
                          className={`join-item btn btn-sm md:btn-md flex-grow ${schedule.days[dayIndex] ? 'btn-primary' : 'btn-neutral text-neutral-content/20'}`}
                          onClick={() =>
                            updateAutoWakeupDay(scheduleIndex, dayIndex, !schedule.days[dayIndex])
                          }
                          disabled={!formData.autowakeupEnabled}
                          aria-pressed={schedule.days[dayIndex]}
                          aria-label={dayNames[dayIndex]}
                          title={dayNames[dayIndex]}
                        >
                          {dayLabel}
                        </button>
                      ))}
                    </div>
                    {/* Delete button */}
                    {autowakeupSchedules.length > 1 ? (
                      <button
                        type='button'
                        onClick={() => removeAutoWakeupSchedule(scheduleIndex)}
                        className='btn btn-ghost btn-sm md:btn-md grow-1'
                        disabled={!formData.autowakeupEnabled}
                        title={t`Delete this schedule`}
                      >
                        <FontAwesomeIcon icon={faTrashCan} className='text-base' />
                      </button>
                    ) : (
                      <div
                        className='btn btn-ghost btn-sm md:btn-md grow-1 cursor-not-allowed opacity-30'
                        title={t`Cannot delete the last schedule`}
                      >
                        <FontAwesomeIcon icon={faTrashCan} className='text-base' />
                      </div>
                    )}
                  </div>
                ))}
                <button
                  type='button'
                  onClick={addAutoWakeupSchedule}
                  className='btn btn-primary btn-sm md:btn-md mt-2'
                  disabled={!formData.autowakeupEnabled}
                  aria-label={t`Add schedule`}
                  title={t`Add schedule`}
                >
                  <FontAwesomeIcon icon={faCalendarDays} />
                </button>
              </div>
            </div>
          </div>
        )}
      </div>

      <div className='bg-base-200 rounded-lg p-4'>
        <div className='flex items-center justify-between'>
          <span className='text-xl font-medium'>HomeKit</span>
          <input
            id='homekit'
            name='homekit'
            value='homekit'
            type='checkbox'
            className='toggle toggle-primary'
            checked={!!formData.homekit}
            onChange={onChange('homekit')}
            aria-label={t`Enable HomeKit`}
          />
        </div>
        {formData.homekit && (
          <div className='border-base-300 mt-4 flex flex-col items-center justify-center gap-4 border-t pt-4'>
            <img src={homekitImage} alt={t`HomeKit Setup Code`} />
            <p className='text-center'>
              <Trans>
                Open the Home app on your iOS device, select Add Accessory, and enter the setup code
                shown above.
              </Trans>
            </p>
          </div>
        )}
      </div>

      <div className='bg-base-200 rounded-lg p-4'>
        <div className='flex items-center justify-between'>
          <span className='text-xl font-medium'>
            <Trans>Boiler Refill Plugin</Trans>
          </span>
          <input
            id='boilerFillActive'
            name='boilerFillActive'
            value='boilerFillActive'
            type='checkbox'
            className='toggle toggle-primary'
            checked={!!formData.boilerFillActive}
            onChange={onChange('boilerFillActive')}
            aria-label={t`Enable Boiler Refill`}
          />
        </div>
        {formData.boilerFillActive && (
          <div className='border-base-300 mt-4 grid grid-cols-2 gap-4 border-t pt-4'>
            <div className='form-control'>
              <label htmlFor='startupFillTime' className='mb-2 block text-sm font-medium'>
                <Trans>On startup (s)</Trans>
              </label>
              <input
                id='startupFillTime'
                name='startupFillTime'
                type='number'
                className='input input-bordered w-full'
                placeholder='0'
                value={formData.startupFillTime}
                onChange={onChange('startupFillTime')}
              />
            </div>
            <div className='form-control'>
              <label htmlFor='steamFillTime' className='mb-2 block text-sm font-medium'>
                <Trans>On steam deactivate (s)</Trans>
              </label>
              <input
                id='steamFillTime'
                name='steamFillTime'
                type='number'
                className='input input-bordered w-full'
                placeholder='0'
                value={formData.steamFillTime}
                onChange={onChange('steamFillTime')}
              />
            </div>
          </div>
        )}
      </div>

      <div className='bg-base-200 rounded-lg p-4'>
        <div className='flex items-center justify-between'>
          <span className='text-xl font-medium'>
            <Trans>Smart Grind Plugin</Trans>
          </span>
          <input
            id='smartGrindActive'
            name='smartGrindActive'
            value='smartGrindActive'
            type='checkbox'
            className='toggle toggle-primary'
            checked={!!formData.smartGrindActive}
            onChange={onChange('smartGrindActive')}
            aria-label={t`Enable Smart Grind`}
          />
        </div>
        {formData.smartGrindActive && (
          <div className='border-base-300 mt-4 space-y-4 border-t pt-4'>
            <p className='text-sm opacity-70'>
              <Trans>
                This feature controls a Tasmota Plug to turn off your grinder after the target has
                been reached.
              </Trans>
            </p>
            <div className='form-control'>
              <label htmlFor='smartGrindIp' className='mb-2 block text-sm font-medium'>
                <Trans>Tasmota IP</Trans>
              </label>
              <input
                id='smartGrindIp'
                name='smartGrindIp'
                type='text'
                className='input input-bordered w-full'
                placeholder='0'
                value={formData.smartGrindIp}
                onChange={onChange('smartGrindIp')}
              />
            </div>
            <div className='form-control'>
              <label htmlFor='smartGrindMode' className='mb-2 block text-sm font-medium'>
                <Trans context='smart grind mode'>Mode</Trans>
              </label>
              <select
                id='smartGrindMode'
                name='smartGrindMode'
                className='select select-bordered w-full'
                onChange={onChange('smartGrindMode')}
              >
                <option value='0' selected={formData.smartGrindMode?.toString() === '0'}>
                  {t`Turn off at target`}
                </option>
                <option value='1' selected={formData.smartGrindMode?.toString() === '1'}>
                  {t`Toggle off and on at target`}
                </option>
                <option value='2' selected={formData.smartGrindMode?.toString() === '2'}>
                  {t`Turn on at start, off at target`}
                </option>
              </select>
            </div>
          </div>
        )}
      </div>

      <div className='bg-base-200 rounded-lg p-4'>
        <div className='flex items-center justify-between'>
          <span className='text-xl font-medium'>Mahlkönig E64 WS Grind-by-Sync</span>
          <input
            id='mahlkonigActive'
            name='mahlkonigActive'
            value='mahlkonigActive'
            type='checkbox'
            className='toggle toggle-primary'
            checked={!!formData.mahlkonigActive}
            onChange={onChange('mahlkonigActive')}
            aria-label={t`Enable Mahlkönig E64 WS Grind-by-Sync`}
          />
        </div>
        {formData.mahlkonigActive && (
          <div className='border-base-300 mt-4 space-y-4 border-t pt-4'>
            <p className='text-sm opacity-70'>
              <Trans>
                Lets an E64 WS start shots with its knob and dial in its grind from the shot time
                and weight. A grind selects the profile mapped to its recipe, and the grinder's
                target weight overrides that profile's weight target. Save and restart to apply.
              </Trans>
            </p>
            <div className='form-control'>
              <span className='mb-2 block text-sm font-medium'>
                <Trans>Machine address for the grinder</Trans>
              </span>
              <code className='bg-base-300 rounded px-2 py-1 text-lg'>
                {formData.deviceIp || t`Not connected to Wi-Fi`}
              </code>
              <div className='mt-2 text-sm opacity-70'>
                <Trans>
                  The grinder reaches GaggiMate at this address, so both need to share a network
                  (the E64 WS is 2.4 GHz only) and the address must not change: reserve it for
                  GaggiMate in your router.
                </Trans>
              </div>
            </div>
            <div className='space-y-1 text-sm opacity-70'>
              <div>
                <Trans>
                  <span className='font-medium'>Pairing:</span> GaggiMate shows up to the grinder as
                  a Xenia machine. Under Settings → Connectivity → Machine To Machine, turn on
                  Enable Xenia, then type the address above into Configuration. Automatic discovery
                  does not see GaggiMate.
                </Trans>
              </div>
              <div>
                <Trans>
                  <span className='font-medium'>Brewing:</span> recipes with GbS turned on report
                  each grind here; pressing the rotary knob afterwards starts the shot.
                </Trans>
              </div>
            </div>
            <MahlkonigRecipeProfiles formData={formData} onChange={onChange} />
            <div className='text-xs opacity-50'>
              <Trans>
                GaggiMate is not affiliated with, endorsed by or supported by Mahlkönig or its
                parent company, the Hemro Group. Mahlkönig, E64 WS, Grind-by-Sync and Xenia are
                trademarks of their respective owners.
              </Trans>
            </div>
          </div>
        )}
      </div>

      <div className='bg-base-200 rounded-lg p-4'>
        <div className='flex items-center justify-between'>
          <span className='text-xl font-medium'>Baratza Sette 270Wi</span>
          <input
            id='setteActive'
            name='setteActive'
            value='setteActive'
            type='checkbox'
            className='toggle toggle-primary'
            checked={!!formData.setteActive}
            onChange={onChange('setteActive')}
            aria-label='Enable Baratza Sette 270Wi'
          />
        </div>
        {formData.setteActive && (
          <div className='border-base-300 mt-4 space-y-4 border-t pt-4'>
            <p className='text-sm opacity-70'>
              Connects to a Sette 270Wi running the grind-events firmware over Bluetooth. The
              grinder's target follows GaggiMate's grind target (turn on "Grind target follows
              profile dose" under General to set it from profiles), and the weight of each completed
              grind becomes the dose of the next shot, so ratio targets follow what you actually
              ground. Saved profiles are never changed. Save and restart to apply.
            </p>
            <SetteGrinder />
          </div>
        )}
      </div>

      <div className='bg-base-200 rounded-lg p-4'>
        <div className='flex items-center justify-between'>
          <span className='text-xl font-medium'>
            <Trans>Home Assistant over MQTT (Deprecated)</Trans>
          </span>
          <input
            id='homeAssistant'
            name='homeAssistant'
            value='homeAssistant'
            type='checkbox'
            className='toggle toggle-primary'
            checked={!!formData.homeAssistant}
            onChange={onChange('homeAssistant')}
            aria-label={t`Enable Home Assistant`}
          />
        </div>
        {formData.homeAssistant && (
          <div className='border-base-300 mt-4 space-y-4 border-t pt-4'>
            <p className='text-sm opacity-70'>
              <Trans>
                This feature allows connection to a Home Assistant or MQTT installation and push the
                current state. This feature is deprecated for usage with Home Assistant. Please see
                the{' '}
                <a
                  href='https://github.com/gaggimate/ha-integration'
                  target='_blank'
                  rel='noreferrer'
                >
                  Home Assistant Integration
                </a>{' '}
                for a more up-to-date solution.
              </Trans>
            </p>
            <div className='form-control'>
              <label htmlFor='haIP' className='mb-2 block text-sm font-medium'>
                <Trans>MQTT IP</Trans>
              </label>
              <input
                id='haIP'
                name='haIP'
                type='text'
                className='input input-bordered w-full'
                placeholder='0'
                value={formData.haIP}
                onChange={onChange('haIP')}
              />
            </div>

            <div className='form-control'>
              <label htmlFor='haPort' className='mb-2 block text-sm font-medium'>
                <Trans>MQTT Port</Trans>
              </label>
              <input
                id='haPort'
                name='haPort'
                type='number'
                className='input input-bordered w-full'
                placeholder='0'
                value={formData.haPort}
                onChange={onChange('haPort')}
              />
            </div>

            <div className='form-control'>
              <label htmlFor='haUser' className='mb-2 block text-sm font-medium'>
                <Trans>MQTT User</Trans>
              </label>
              <input
                id='haUser'
                name='haUser'
                type='text'
                className='input input-bordered w-full'
                placeholder={t`user`}
                value={formData.haUser}
                onChange={onChange('haUser')}
              />
            </div>

            <div className='form-control'>
              <label htmlFor='haPassword' className='mb-2 block text-sm font-medium'>
                <Trans>MQTT Password</Trans>
              </label>
              <input
                id='haPassword'
                name='haPassword'
                type='password'
                className='input input-bordered w-full'
                placeholder={t`password`}
                value={formData.haPassword}
                onChange={onChange('haPassword')}
              />
            </div>
            <div className='form-control'>
              <label htmlFor='haTopic' className='mb-2 block text-sm font-medium'>
                <Trans>Home Assistant Discovery Topic</Trans>
              </label>
              <input
                id='haTopic'
                name='haTopic'
                type='text'
                className='input input-bordered w-full'
                value={formData.haTopic}
                onChange={onChange('haTopic')}
              />
            </div>
          </div>
        )}
      </div>

      {gearpumpAddon.value && (
        <div className='bg-base-200 rounded-lg p-4'>
          <div className='flex items-center justify-between'>
            <span className='text-xl font-medium'>
              <Trans>BLDC Pump Settings</Trans>
            </span>
          </div>
          <div className='border-base-300 mt-4 space-y-4 border-t pt-4'>
            <p className='text-sm opacity-70'>
              <Trans>
                The BLDC pump addon was detected in your system. You can change the pump control
                characteristics using the values below.
              </Trans>
            </p>

            <div className='form-control'>
              <label htmlFor='commutationGain' className='mb-2 block text-sm font-medium'>
                <Trans>Commutation Gain</Trans>
              </label>
              <input
                id='commutationGain'
                name='commutationGain'
                type='number'
                className='input input-bordered w-full'
                placeholder='0'
                min='0'
                max='100'
                step='any'
                value={formData.commutationGain?.toString()}
                onChange={onChange('commutationGain')}
              />
            </div>

            <div className='form-control'>
              <label htmlFor='convergenceGain' className='mb-2 block text-sm font-medium'>
                <Trans>Convergence Gain</Trans>
              </label>
              <input
                id='convergenceGain'
                name='convergenceGain'
                type='number'
                className='input input-bordered w-full'
                placeholder='0'
                min='0'
                max='100'
                step='any'
                value={formData.convergenceGain?.toString()}
                onChange={onChange('convergenceGain')}
              />
            </div>

            <div className='form-control'>
              <label htmlFor='integralGain' className='mb-2 block text-sm font-medium'>
                <Trans>Integral Gain</Trans>
              </label>
              <input
                id='integralGain'
                name='integralGain'
                type='number'
                className='input input-bordered w-full'
                placeholder='0'
                min='0'
                max='100'
                step='any'
                value={formData.integralGain?.toString()}
                onChange={onChange('integralGain')}
              />
            </div>
            <div className='form-control'>
              <label htmlFor='maxPumpPower' className='mb-2 block text-sm font-medium'>
                <Trans>Maximum Pump Power (0 - 1)</Trans>
              </label>
              <input
                id='maxPumpPower'
                name='maxPumpPower'
                type='number'
                placeholder='0'
                min='0'
                max='1'
                step='any'
                className='input input-bordered w-full'
                value={formData.maxPumpPower?.toString()}
                onChange={onChange('maxPumpPower')}
              />
            </div>
            <div className='form-control'>
              <label htmlFor='pumpSlipCoeffs' className='mb-2 block text-sm font-medium'>
                <Trans>Pump Slip Coefficients</Trans>
              </label>
              <input
                id='pumpSlipCoeffs'
                name='pumpSlipCoeffs'
                type='text'
                className='input input-bordered w-full'
                placeholder='0,0,0,0'
                value={formData.pumpSlipCoeffs}
                onChange={onChange('pumpSlipCoeffs')}
              />
              <span className='mt-1 text-xs opacity-70'>
                <Trans>
                  Pressure polynomial (a,b,c,d) for vane-/gear-pump internal leakage. Leave at
                  0,0,0,0 if uncalibrated.
                </Trans>
              </span>
            </div>
          </div>
        </div>
      )}
    </div>
  );
}
