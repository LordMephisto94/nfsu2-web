// Shared-engine split-screen input. Each physical pad owns at most one seat.
// START claims a seat; disconnecting preserves its place until a controller
// explicitly claims it. Joining buttons must be released before driving.
import type { PadSnapshot } from './gamepad';
import {
  INPUT_LOCAL_PADS, IM_LOCAL_WORDS, IM_OFF_LOCAL_PADS, IM_LOCAL_SEQ, IM_LOCAL_CONNECTED,
  IM_LOCAL_BUTTONS, IM_LOCAL_FAMILY, IM_LOCAL_AXIS, IM_LOCAL_NAME, IM_LOCAL_INDEX, IM_LOCAL_JOIN, IM_LOCAL_COOP, IM_LOCAL_CLAIM,
} from './proto';

type Owner = { index: number; id: string; waiting: boolean };
export class LocalSeats {
  owners: (Owner | null)[] = Array.from({ length: INPUT_LOCAL_PADS }, () => null);
  private starts = new Map<number, { id: string; down: boolean }>();
  readonly joins = Array(INPUT_LOCAL_PADS).fill(0);
  private lastClaim = -1;
  constructor(private log: (message: string) => void = () => {}) {}

  poll(pads: readonly (Gamepad | null)[], claimSeat = -1): (Gamepad | null)[] {
    const live = pads.filter((g): g is Gamepad => !!g?.connected && g.mapping === 'standard');
    const present = (o: Owner | null) => o && live.find(g => g.index === o.index && g.id === o.id);
    // A fresh native Press Start claim starts a new assignment window. Never
    // reuse a Start edge from before this window or auto-assign it elsewhere.
    if (claimSeat !== this.lastClaim) {
      this.log(`[split-debug] assignment window ${this.lastClaim} -> ${claimSeat}`);
      this.lastClaim = claimSeat;
      for (const g of live) this.starts.set(g.index, { id: g.id,
        down: !!g.buttons[9]?.pressed || (g.buttons[9]?.value ?? 0) > 0.5 });
    }
    for (const g of live) {
      const down = !!g.buttons[9]?.pressed || (g.buttons[9]?.value ?? 0) > 0.5;
      const was = this.starts.get(g.index);
      if (down && !(was?.id === g.id && was.down)) {
        const existing = this.owners.findIndex(o => o?.index === g.index && o.id === g.id);
        // During native Press Start, the requested seat takes precedence over
        // an old assignment. Never steal a controller from a different seat
        // unless that controller is being explicitly reclaimed for this seat.
        let seat = claimSeat >= 0 && claimSeat < this.owners.length ? claimSeat : existing;
        // Never allocate a new player without an explicit native claim.
        // Existing owners may still use Start for their own controls.
        if (seat >= 0) {
          // A new native Press Start screen may reclaim a pad which was
          // auto-assigned to another seat before that seat actually joined.
          if (claimSeat >= 0) {
            // One physical pad can never control two logical players. Also
            // evict the previous owner of the waiting seat explicitly.
            for (let i = 0; i < this.owners.length; i++) {
              if (i !== seat && this.owners[i]?.index === g.index && this.owners[i]?.id === g.id)
                this.owners[i] = null;
            }
          }
          this.owners[seat] = { index: g.index, id: g.id, waiting: true };
        }
        if (seat >= 0) this.joins[seat] = (this.joins[seat] + 1) >>> 0;
        this.log(`[split-debug] physical pad ${g.index} START; prior seat ${existing}, claim ${claimSeat}, assigned seat ${seat}, join ${seat >= 0 ? this.joins[seat] : 'none'}`);
      }
      this.starts.set(g.index, { id: g.id, down });
    }
    for (const index of this.starts.keys()) if (!live.some(g => g.index === index)) this.starts.delete(index);
    return this.owners.map(o => {
      const g = present(o);
      if (!o || !g) { if (o) o.waiting = true; return null; }
      if (o.waiting) {
        if (g.buttons.some(b => b.pressed || b.value > 0.5)) return null;
        o.waiting = false;
      }
      return g;
    });
  }
}

/** One record per seat in the SAME game's shared memory, not six processes. */
export class LocalPadWriter {
  private seats: LocalSeats;
  private words: Int32Array;
  private bits = new Float32Array(1);
  private ints = new Int32Array(this.bits.buffer);
  private previousClaim = -2;
  constructor(buffer: SharedArrayBuffer | ArrayBuffer, base: number, private log: (message: string) => void = () => {}) {
    this.seats = new LocalSeats(log);
    this.words = new Int32Array(buffer, base + IM_OFF_LOCAL_PADS, INPUT_LOCAL_PADS * IM_LOCAL_WORDS);
  }
  /** the controllers (Gamepad.index) the game keeps for co-op free roam's player 2: they drive nothing else */
  reserved(): Set<number> {
    const out = new Set<number>();
    const claiming = this.seats.owners.some((_, seat) => Atomics.load(this.words, seat * IM_LOCAL_WORDS + IM_LOCAL_CLAIM) === 1);
    this.seats.owners.forEach((o, seat) => { if (o && (claiming || Atomics.load(this.words, seat * IM_LOCAL_WORDS + IM_LOCAL_COOP) === 1)) out.add(o.index); });
    return out;
  }
  poll(pads: readonly (Gamepad | null)[], read: (g: Gamepad) => PadSnapshot, muted = false) {
    const requested = Array.from({ length: INPUT_LOCAL_PADS }, (_, seat) =>
      Atomics.load(this.words, seat * IM_LOCAL_WORDS + IM_LOCAL_CLAIM));
    const claimSeat = requested.findIndex(v => v === 1);
    if (claimSeat !== this.previousClaim) {
      this.log(`[split-debug] browser claim ${this.previousClaim} -> ${claimSeat}; flags ${requested.join(',')}`);
      this.previousClaim = claimSeat;
    }
    this.seats.poll(pads, claimSeat).forEach((g, seat) => {
      const state = g && !muted ? read(g) : null;
      const at = seat * IM_LOCAL_WORDS, w = this.words;
      const seq = Atomics.load(w, at + IM_LOCAL_SEQ);
      Atomics.store(w, at + IM_LOCAL_SEQ, (seq + 1) | 0);
      Atomics.store(w, at + IM_LOCAL_CONNECTED, state ? 1 : 0);
      Atomics.store(w, at + IM_LOCAL_BUTTONS, state?.buttons ?? 0);
      Atomics.store(w, at + IM_LOCAL_FAMILY, state?.family ?? 0);
      Atomics.store(w, at + IM_LOCAL_INDEX, g?.index ?? -1);
      Atomics.store(w, at + IM_LOCAL_JOIN, this.seats.joins[seat]);
      for (let i = 0; i < 6; i++) {
        this.bits[0] = state?.axes[i] ?? 0;
        Atomics.store(w, at + IM_LOCAL_AXIS + i, this.ints[0]);
      }
      const name = (state?.name ?? '').slice(0, 63);
      for (let i = 0; i < 16; i++) {
        let value = 0;
        for (let b = 0; b < 4; b++) value |= ((name.charCodeAt(i * 4 + b) || 0) & 255) << (b * 8);
        Atomics.store(w, at + IM_LOCAL_NAME + i, value);
      }
      Atomics.store(w, at + IM_LOCAL_SEQ, (seq + 2) | 0);
    });
  }
}
