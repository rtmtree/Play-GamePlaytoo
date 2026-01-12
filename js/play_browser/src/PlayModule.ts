// import Play from "./ram/dynamic/Play";
// import PlayDyna from "./ram/PlayDyna";
import Play from "./Play";
// Note: Play300.js and Play600.js will be imported when they exist
// For now, we'll use Play as fallback
import DiscImageDevice from "./DiscImageDevice";

export let PlayModule: any = null;

let module_overrides = {
    locateFile: function (path: string) {
        const baseURL = window.location.origin + window.location.pathname.substring(0, window.location.pathname.lastIndexOf("/"));
        return baseURL + '/' + path;
    },
    mainScriptUrlOrBlob: "",
};

// Parse URL parameter to get RAM setting
function getRAMFromURL(): string | null {
    const urlParams = new URLSearchParams(window.location.search);
    return urlParams.get('ram');
}

// Load Play module dynamically based on RAM setting
async function loadPlayModule(ramParam: string | null): Promise<{ moduleFunction: any; jsFileName: string }> {
    let playModuleFunction: any;
    let playJSFileName: string;

    console.log('Loading Play module with RAM setting:', ramParam);

    // Select module and JS file based on ?ram= parameter
    if (ramParam === '300') {
        playJSFileName = 'Play300.js';
        // try {
            // Try dynamic import for Play300.js
            // @ts-ignore - Play300.js may not exist yet, will fallback to Play.js
            const Play300Module = await import('./Play300.js');
            playModuleFunction = Play300Module.default || Play300Module;
            if (!playModuleFunction || typeof playModuleFunction !== 'function') {
                throw new Error('Play300 module not valid');
            }
            console.log('Successfully loaded Play300.js');
        // } catch (error) {
        //     // Fallback to default Play.js if Play300.js doesn't exist
        //     console.warn('Play300.js not found, falling back to Play.js');
        //     playModuleFunction = Play;
        //     playJSFileName = 'Play.js';
        // }
    } else if (ramParam === '600') {
        playJSFileName = 'Play600.js';
        // try {
            // Try dynamic import for Play600.js
            // @ts-ignore - Play600.js may not exist yet, will fallback to Play.js
            const Play600Module = await import('./Play600.js');
            playModuleFunction = Play600Module.default || Play600Module;
            if (!playModuleFunction || typeof playModuleFunction !== 'function') {
                throw new Error('Play600 module not valid');
            }
            console.log('Successfully loaded Play600.js');
        // } catch (error) {
        //     // Fallback to default Play.js if Play600.js doesn't exist
        //     console.warn('Play600.js not found, falling back to Play.js');
        //     playModuleFunction = Play;
        //     playJSFileName = 'Play.js';
        // }
    } else if (ramParam === '150') {
        playJSFileName = 'Play150.js';
        // try {
        // Try dynamic import for Play150.js
        // @ts-ignore - Play150.js may not exist yet, will fallback to Play.js
        const Play150Module = await import('./' + playJSFileName);
        playModuleFunction = Play150Module.default || Play150Module;
        if (!playModuleFunction || typeof playModuleFunction !== 'function') {
            throw new Error('Play150 module not valid');
        }
        console.log('Successfully loaded Play150.js');
        // } catch (error) {
        //     // Fallback to default Play.js if Play150.js doesn't exist
        //     console.warn('Play150.js not found, falling back to Play.js');
        //     playModuleFunction = Play;
        //     playJSFileName = 'Play.js';
        // }
    } else if (ramParam === '900') {
        playJSFileName = 'Play900.js';
        try {
            // Try dynamic import for Play900.js
            // @ts-ignore - Play900.js may not exist yet, will fallback to Play.js
            const Play900Module = await import('./Play900.js');
            playModuleFunction = Play900Module.default || Play900Module;
            if (!playModuleFunction || typeof playModuleFunction !== 'function') {
                throw new Error('Play900 module not valid');
            }
            console.log('Successfully loaded Play900.js');
        } catch (error) {
            // Fallback to default Play.js if Play900.js doesn't exist
            console.warn('Play900.js not found, falling back to Play.js');
            playModuleFunction = Play;
            playJSFileName = 'Play.js';
        }
    } else if (ramParam === 'dyna') {
        playJSFileName = 'PlayDyna.js';
        // try {
        // Try dynamic import for PlayDyna.js
        // @ts-ignore - PlayDyna.js may not exist yet, will fallback to Play.js
        const PlayDynamicModule = await import('./' + playJSFileName);
        playModuleFunction = PlayDynamicModule.default || PlayDynamicModule;
        if (!playModuleFunction || typeof playModuleFunction !== 'function') {
            throw new Error('PlayDyna module not valid');
        }
        //     console.log('Successfully loaded PlayDyna.js');
        // } catch (error) {
        //     // Fallback to default Play.js if PlayDyna.js doesn't exist
        //     console.warn('PlayDyna.js not found, falling back to Play.js');
        //     playModuleFunction = Play;
        //     playJSFileName = 'Play.js';
        // }
    }
    else {
        // Default: use Play.js when ?ram= is not present or has other value
        playModuleFunction = Play;
        playJSFileName = 'Play.js';
    }

    return { moduleFunction: playModuleFunction, jsFileName: playJSFileName };
}

export let initPlayModule = async function () {
    // Log SharedArrayBuffer availability for debugging (not required since we disabled pthreads)
    console.log('SharedArrayBuffer available:', typeof SharedArrayBuffer !== 'undefined');

    // Get RAM setting from URL parameter
    const ramParam = getRAMFromURL();

    // Load the appropriate Play module based on RAM setting
    const { moduleFunction: playModuleFunction, jsFileName: playJSFileName } = await loadPlayModule(ramParam);

    console.log(`Loading Play WASM module with RAM setting: ${ramParam || 'default (not specified)'}, using ${playJSFileName}`);

    module_overrides.mainScriptUrlOrBlob = module_overrides.locateFile(playJSFileName);

    try {
        console.log('Loading Play WASM module...');
        PlayModule = await playModuleFunction(module_overrides);
        console.log('Play WASM module loaded successfully');

        // Verify WASM memory is initialized
        if (!PlayModule) {
            throw new Error('PlayModule is null or undefined');
        }

        if (!PlayModule.HEAP8) {
            throw new Error('WASM module memory (HEAP8) not properly initialized');
        }

        console.log('WASM memory initialized, HEAP8 size:', PlayModule.HEAP8.length);

        console.log('Creating /work directory...');
        PlayModule.FS.mkdir("/work");

        console.log('Initializing DiscImageDevice...');
        PlayModule.discImageDevice = new DiscImageDevice(PlayModule);

        console.log('Calling initVm...');
        // Wait for module to be fully initialized
        if (!PlayModule.ready) {
            await PlayModule;
        }

        // Wait for worker threads to finish loading before calling initVm
        // This helps prevent "Out of bounds memory access" errors in worker threads
        // on low RAM devices like iPhone Air where worker initialization takes longer
        console.log('Waiting for worker threads to initialize...');

        // Detect iPhone/iOS devices which may need longer initialization time
        const isIOS = /iPad|iPhone|iPod/.test(navigator.userAgent);
        const isLowRAMDevice = isIOS || (navigator.hardwareConcurrency && navigator.hardwareConcurrency <= 4);

        // Longer delay for iOS/low RAM devices to ensure workers are fully initialized
        const delayMs = isLowRAMDevice ? 1500 : 500;
        await new Promise(resolve => setTimeout(resolve, delayMs));

        // Additional check: wait for any pending worker operations
        if (PlayModule.PThread && PlayModule.PThread.unusedWorkers) {
            // Wait for all unused workers to be loaded (they're initialized on preRun)
            let maxWaitTime = 3000; // Max 3 seconds
            const startTime = Date.now();
            while (Date.now() - startTime < maxWaitTime) {
                const allWorkersLoaded = PlayModule.PThread.unusedWorkers.every((worker: any) => worker.loaded);
                if (allWorkersLoaded) {
                    console.log('All worker threads loaded');
                    break;
                }
                await new Promise(resolve => setTimeout(resolve, 100));
            }
        }

        // Ensure canvas exists before calling initVm (initVm creates WebGL context on #outputCanvas)
        // Note: Do NOT create a WebGL context here - let initVm() create it via emscripten_webgl_create_context
        let canvas = document.getElementById('outputCanvas') as HTMLCanvasElement;
        if (!canvas) {
            console.warn('Canvas #outputCanvas not found, creating it...');
            canvas = document.createElement('canvas');
            canvas.id = 'outputCanvas';
            // Set minimum size for WebGL context creation (some browsers require non-zero size)
            // Reduced from 640x480 to 480x360 for low RAM device compatibility
            canvas.width = 480;
            canvas.height = 360;
            // Make canvas visible (some browsers require canvas to be in viewport for WebGL)
            // We'll hide it with CSS if needed, but keep it in the layout
            canvas.style.position = 'fixed';
            canvas.style.left = '0';
            canvas.style.top = '0';
            canvas.style.width = '480px';
            canvas.style.height = '360px';
            canvas.style.zIndex = '-1'; // Behind everything
            canvas.style.pointerEvents = 'none'; // Don't intercept mouse events
            document.body.appendChild(canvas);
            console.log('Canvas created with size:', canvas.width, 'x', canvas.height);
        } else {
            // Ensure canvas has valid dimensions
            if (canvas.width === 0 || canvas.height === 0) {
                canvas.width = 480;
                canvas.height = 360;
                console.log('Canvas dimensions updated to:', canvas.width, 'x', canvas.height);
            }
        }

        // Verify WebGL is available (but don't create a context - let initVm do that)
        const testCanvas = document.createElement('canvas');
        const testGl = testCanvas.getContext('webgl2') || testCanvas.getContext('webgl');
        if (!testGl) {
            throw new Error('WebGL is not supported in this environment. Cannot initialize Play! emulator.');
        }
        console.log('WebGL support verified, WebGL version:', testGl.getParameter(testGl.VERSION));

        // Verify canvas is findable by CSS selector (emscripten_webgl_create_context uses querySelector)
        const canvasBySelector = document.querySelector('#outputCanvas');
        if (!canvasBySelector) {
            throw new Error('Canvas #outputCanvas not found by CSS selector. This will cause emscripten_webgl_create_context to fail.');
        }
        if (canvasBySelector !== canvas) {
            console.warn('Warning: Multiple elements found with id "outputCanvas"');
        }
        console.log('Canvas #outputCanvas verified, accessible by CSS selector');

        // Try embind export first (initVm), then EXPORTED_FUNCTIONS (_initVm), then ccall
        let initResult;
        if (typeof PlayModule.initVm === 'function') {
            // Call via embind (preferred when using --bind)
            // initVm is void, but might throw on failure (e.g., assert failure in WebGL context creation)
            // Retry logic for "unwind" errors which can occur when worker threads aren't ready
            let retryCount = 0;
            const maxRetries = isLowRAMDevice ? 3 : 1;
            const retryDelay = 1000; // 1 second between retries

            while (retryCount <= maxRetries) {
                try {
                    initResult = PlayModule.initVm();
                    // initVm is void, so initResult should be undefined
                    // If it returns a number, something went wrong
                    if (typeof initResult === 'number') {
                        console.error('initVm returned unexpected numeric value:', initResult);
                        throw new Error(`initVm returned unexpected value: ${initResult}. This might indicate a WebGL context creation failure.`);
                    }
                    // Success - break out of retry loop
                    break;
                } catch (error: any) {
                    // Check if this is an "unwind" error (C++ exception indicating worker thread issue)
                    const isUnwindError = error === 'unwind' || String(error) === 'unwind';

                    if (isUnwindError && retryCount < maxRetries) {
                        retryCount++;
                        console.warn(`initVm failed with unwind error (attempt ${retryCount}/${maxRetries + 1}), waiting ${retryDelay}ms before retry...`);
                        await new Promise(resolve => setTimeout(resolve, retryDelay));
                        continue; // Retry
                    }
                    // Log detailed error information
                    console.error('initVm threw an error:', error);
                    console.error('Error details:', {
                        type: typeof error,
                        value: error,
                        message: error?.message,
                        name: error?.name,
                        stack: error?.stack
                    });

                    // Handle numeric errors (Emscripten assertion failures often throw numbers)
                    if (typeof error === 'number') {
                        // Convert to hex for debugging
                        const errorHex = '0x' + error.toString(16).toUpperCase();
                        console.error(`Numeric error code: ${error} (${errorHex})`);

                        // Check if canvas is actually accessible
                        const canvasCheck = document.getElementById('outputCanvas');
                        if (!canvasCheck) {
                            throw new Error(`initVm failed: Canvas #outputCanvas not found in DOM. Error code: ${error} (${errorHex}). This indicates WebGL context creation failed because the canvas element is missing.`);
                        }

                        // Check canvas dimensions
                        const canvasEl = canvasCheck as HTMLCanvasElement;
                        if (canvasEl.width === 0 || canvasEl.height === 0) {
                            throw new Error(`initVm failed: Canvas #outputCanvas has invalid dimensions (${canvasEl.width}x${canvasEl.height}). Error code: ${error} (${errorHex}). Canvas must have non-zero dimensions for WebGL context creation.`);
                        }

                        // Check if canvas is in the document
                        if (!document.body.contains(canvasCheck)) {
                            throw new Error(`initVm failed: Canvas #outputCanvas is not in the document body. Error code: ${error} (${errorHex}). Canvas must be attached to the DOM for WebGL context creation.`);
                        }

                        throw new Error(`initVm failed: WebGL context creation failed with error code ${error} (${errorHex}). Canvas exists with dimensions ${canvasEl.width}x${canvasEl.height}. This might indicate WebGL 2.0 is not available or the context attributes are incompatible.`);
                    }

                    // Check for Emscripten-specific error patterns
                    const errorStr = String(error);
                    if (errorStr.includes('assert') || errorStr.includes('Assertion')) {
                        throw new Error(`initVm assertion failed. This usually means WebGL context creation failed. Error: ${errorStr}`);
                    }

                    // Re-throw with more context
                    if (error instanceof Error) {
                        throw new Error(`initVm failed: ${error.message}. The canvas #outputCanvas exists and has dimensions ${canvas.width}x${canvas.height}.`);
                    } else {
                        throw new Error(`initVm failed with unexpected error type: ${typeof error}, value: ${error}`);
                    }
                }
            }
            // If we exhausted retries, the error was already thrown above
        } else if (typeof PlayModule._initVm === 'function') {
            // Call via EXPORTED_FUNCTIONS
            try {
                initResult = PlayModule._initVm();
                if (typeof initResult === 'number' && (initResult < 0 || initResult > 1000000)) {
                    throw new Error(`_initVm returned error code: ${initResult}`);
                }
            } catch (error: any) {
                throw new Error(`_initVm failed: ${error}`);
            }
        } else if (typeof PlayModule.ccall === 'function') {
            // Fallback to ccall
            try {
                initResult = PlayModule.ccall('initVm', null, [], []);
                if (typeof initResult === 'number' && (initResult < 0 || initResult > 1000000)) {
                    throw new Error(`initVm (via ccall) returned error code: ${initResult}`);
                }
            } catch (error: any) {
                throw new Error(`initVm (via ccall) failed: ${error}`);
            }
        } else {
            throw new Error('initVm function not found. Available methods: ' + Object.keys(PlayModule).filter(k => k.includes('init') || k === '_initVm').join(', '));
        }
        console.log('initVm completed successfully');

    } catch (error) {
        console.error('Error initializing PlayModule:', error);
        console.error('Error type:', typeof error);
        console.error('Error value:', error);

        // Log additional debugging info
        if (error instanceof Error) {
            console.error('Error message:', error.message);
            console.error('Error stack:', error.stack);
        } else if (typeof error === 'number') {
            // Emscripten error code
            console.error('Emscripten error code:', error);
            console.error('This might be a memory allocation error or initialization failure');
        } else {
            console.error('Error stringified:', JSON.stringify(error));
        }

        // Log PlayModule state if available
        if (PlayModule) {
            console.error('PlayModule state:', {
                hasHEAP8: !!PlayModule.HEAP8,
                hasHEAP32: !!PlayModule.HEAP32,
                hasFS: !!PlayModule.FS,
                hasccall: !!PlayModule.ccall
            });
        }

        throw error;
    }
};
