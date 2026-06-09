import { $ } from '../$';
import { assertInternalConstructor, def, proto } from '../utils';
import type {
	GamepadHapticActuatorType,
	GamepadMappingType,
	GamepadEffectParameters,
} from '../types';

/**
 * Defines an individual gamepad or other controller, allowing access
 * to information such as button presses, axis positions, and id.
 *
 * @see https://developer.mozilla.org/docs/Web/API/Gamepad
 */
export class Gamepad implements globalThis.Gamepad {
	readonly axes!: readonly number[];
	readonly buttons!: readonly GamepadButton[];
	readonly connected!: boolean;
	readonly id!: string;
	readonly index!: number;
	readonly mapping!: GamepadMappingType;
	readonly timestamp!: number;
	readonly vibrationActuator!: GamepadHapticActuator;

	// Non-standard
	readonly deviceType!: number;
	readonly rawButtons!: bigint;
	readonly styleSet!: number;

	/**
	 * @ignore
	 */
	constructor() {
		assertInternalConstructor(arguments);
	}
}
def(Gamepad);
$.gamepadInit(Gamepad);

/**
 * Defines an individual button of a gamepad or other controller, allowing access
 * to the current state of different types of buttons available on the control device.
 *
 * @see https://developer.mozilla.org/docs/Web/API/GamepadButton
 */
export class GamepadButton implements globalThis.GamepadButton {
	pressed!: boolean;
	touched!: boolean;
	value!: number;

	/**
	 * @ignore
	 */
	constructor() {
		assertInternalConstructor(arguments);
	}
}
def(GamepadButton);
$.gamepadButtonInit(GamepadButton);

/**
 * Represents hardware in the controller designed to provide haptic feedback
 * to the user (if available), most commonly vibration hardware.
 *
 * @see https://developer.mozilla.org/docs/Web/API/GamepadHapticActuator
 */
export class GamepadHapticActuator implements globalThis.GamepadHapticActuator {
	readonly type!: GamepadHapticActuatorType;

	playEffect(
		type: 'dual-rumble',
		params?: GamepadEffectParameters,
	): Promise<GamepadHapticsResult> {
		throw new Error('Method not implemented.');
	}

	reset(): Promise<GamepadHapticsResult> {
		throw new Error('Method not implemented.');
	}

	pulse(duration: number, delay?: number): void {
		throw new Error('Method not implemented.');
	}
}
def(GamepadHapticActuator);

export function gamepadNew(index: number) {
	const g = proto($.gamepadNew(index), Gamepad);
	// @ts-expect-error Readonly prop
	g.mapping = 'standard';
	// Extended button layout. The Web Gamepad "standard" mapping has
	// 17 buttons; nxjs exposes 22 to include the Switch-specific
	// side-joycon SL/SR buttons (indices 16-19) and the Capture /
	// HOME system buttons (20-21). Pages assuming standard-only
	// mapping will simply not iterate past their expected count;
	// pages that care about the extras can index them directly. See
	// `nxjs-source/source/gamepad.c standard_button_masks` for the
	// per-index mapping (source of truth).
	const NX_BUTTON_COUNT = 22;
	// @ts-expect-error Readonly prop
	g.buttons = Array(NX_BUTTON_COUNT);
	for (let i = 0; i < NX_BUTTON_COUNT; i++) {
		// @ts-expect-error Readonly array
		g.buttons[i] = proto($.gamepadButtonNew(g, i), GamepadButton);
	}
	return g;
}

export const gamepads: Gamepad[] = Array(8)
	.fill(0)
	.map((_, i) => gamepadNew(i));
