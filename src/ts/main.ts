import '../scss/styles.scss'

import { Modal, Toast } from 'bootstrap'
import $ from 'jquery'
import { Media_Type_Flags, Image_Format_Flags, Factorio_Icon_Resolution } from './common';
import Module, { MyModule } from 'display_gen';

;(() => {
    'use strict'

    // const displayWorker = new Worker(new URL('display_worker.ts', import.meta.url), { type: 'module' })

    const blueprintModal = new Modal('#blueprint-modal')
    const blueprintErrorToast = new Toast('#blueprint-error-toast')

    const MAX_FILE_LEN = 255
    const DEFAULT_LONG_IMG_DIM = 64

    var wasmModule: MyModule

    // Updated by wasmModule when ready
    var isWasmReady = false

    // The currently loaded media type
    var loadedMediaType = Media_Type_Flags.NONE_TYPE

    var lastBlueprintString: string = ''
    var lastOutputFilepath: string = ''

    function fitResolutionToImage(changeW: boolean, changeH: boolean) {
        let media_width, media_height;

        if (loadedMediaType & Media_Type_Flags.VIDEO_TYPE) {
            let video = $('#input-video')[0] as HTMLVideoElement
            media_width = video.height;
            media_height = video.height;
        } else if (loadedMediaType & Media_Type_Flags.IMAGE_TYPE) {
            let image = $('#input-image')[0] as HTMLImageElement
            media_width = image.height;
            media_height = image.height;
        } else {
            // No valid media is loaded; do nothing
            return;
        }
        let resetDims = !changeW && !changeH
        if (resetDims) {
            if (media_width > media_height) {
                $('#grid-width').val(DEFAULT_LONG_IMG_DIM)
                $('#grid-height').val(Math.round((DEFAULT_LONG_IMG_DIM * media_height) / media_width))
            } else {
                $('#grid-height').val(DEFAULT_LONG_IMG_DIM)
                $('#grid-width').val(Math.round((DEFAULT_LONG_IMG_DIM * media_width) / media_height))
            }
        } else {
            if (changeW) {
                let gridHeight = parseInt($('#grid-height').val() as string)
                $('#grid-width').val(Math.round((gridHeight * media_width) / media_height))
            } else {
                let gridWidth = parseInt($('#grid-width').val() as string)
                $('#grid-height').val(Math.round((gridWidth * media_height) / media_width))
            }
        }
    }

    async function generateBlueprint() {
        // Disable form while generating the blueprint
        setFormBusy(true)

        try {
            let outputTypeString = ''
            if (loadedMediaType & Media_Type_Flags.IMAGE_TYPE) {
                // Get parameters
                let gridWidth = parseInt($('#grid-width').val() as string)
                let gridHeight = parseInt($('#grid-height').val() as string)
                let gridSpacing = parseInt($('#grid-spacing').val() as string)
                let imgBrightness = parseInt($('#image-brightness').val() as string)
                let simResolution = parseInt($('#sim-resolution').val() as string) as Factorio_Icon_Resolution

                let useDithering = $('#use-dithering').is(':checked')
                let useAlpha = $('#use-alpha').is(':checked')
                let useBinary = $('#use-binary').is(':checked')

                let fmt_flags = (
                    (useBinary    ? 0x01 : 0x00) | 
                    (useDithering ? 0x02 : 0x00) | 
                    (useAlpha     ? 0x04 : 0x00)
                ) as Image_Format_Flags

                wasmModule._Set_Image_Config(gridWidth, gridHeight, gridSpacing, fmt_flags, simResolution)
                outputTypeString = 'image/png'
            }

            if (loadedMediaType & Media_Type_Flags.AUDIO_TYPE) {
                // TODO: Add parameters for audio
                outputTypeString = 'audio/ogg'
            }

            if (loadedMediaType & Media_Type_Flags.VIDEO_TYPE) {
                // TODO: Add parameters for video
                outputTypeString = 'video/webm'
            }

            // Process image
            if (wasmModule._Process_Media()) {
                // Save the blueprint string
                lastBlueprintString = wasmModule.UTF8ToString(wasmModule._Get_Blueprint_String())
                lastOutputFilepath = wasmModule.UTF8ToString(wasmModule._Get_Preview_Filepath())
                
                const previewBlob = new Blob([wasmModule.FS.readFile(lastOutputFilepath)], {'type': outputTypeString})
                const previewUrl = URL.createObjectURL(previewBlob)

                if (loadedMediaType & Media_Type_Flags.VIDEO_TYPE) {
                    ($('#output-video').attr('src', previewUrl)[0] as HTMLVideoElement).load()
                } else if (loadedMediaType & Media_Type_Flags.IMAGE_TYPE) {
                    $('#output-image').attr('src', previewUrl)
                } else if (loadedMediaType & Media_Type_Flags.AUDIO_TYPE) {
                    ($('#output-audio').attr('src', previewUrl)[0] as HTMLAudioElement).load()
                } 
            } else {
                throw Error("Failed to generate blueprint string")
            }
            
        } catch (error) {
            showErrorToast(error as string)
        } finally {
            // Allow the form to be modified again
            setFormBusy(false)
        }
    }

    function validateForm() {
        // Validate fields
        $('#generate-form').addClass('was-validated')
        // TODO: Add message if wasm is not ready
        return ($('#generate-form').get(0) as HTMLFormElement).checkValidity() && isWasmReady && loadedMediaType != Media_Type_Flags.NONE_TYPE;
    }

    function showErrorToast(error: string) {
        $('#collapseToastErrorMsg').text(error)
        blueprintErrorToast.show()
    }

    function showBlueprintString() {
        navigator.clipboard.writeText(lastBlueprintString)
        $('#blueprint-string').val(lastBlueprintString)
        blueprintModal.show()
    }

    function setFormBusy(isBusy: boolean) {
        if (isBusy) {
            $('#generate-btn').removeClass('idle')
            $('#generate-btn-msg').text('Generating...')
        } else {
            $('#generate-btn').addClass('idle')
            $('#generate-btn-msg').text('Show blueprint')
            $('#generate-btn').prop('disabled', false)
        }
        $('#generate-fieldset').prop('disabled', isBusy)
    }

    function clearBlueprintString() {
        lastBlueprintString = ''
        $('#generate-btn-msg').text('Generate blueprint')
    }

    /* Load WebAssembly module */

    $(window).on('load', async (e) => {
        await Module().then((module) => {
            wasmModule = module
            // TODO: set at the end of WASM main()
            isWasmReady = true
        })
    })

    /* Handle input file */

    const inputFileReader = new FileReader();
    var inputFileName: string = '';

    function read_media(){
        let mediaInput = $('#image-file')[0] as HTMLInputElement
        let mediaFile = mediaInput.files?.item(0)
        if (mediaFile != null) {
            if (lastOutputFilepath != '') {
                // Unload previous output file
                wasmModule.FS.unlink(lastOutputFilepath)
                lastOutputFilepath = ''
            }
            if (inputFileName != '') {
                // Unload previous input file
                wasmModule.FS.unlink(inputFileName)
            }
            inputFileName = mediaFile.name
            inputFileReader.readAsArrayBuffer(mediaFile)
        } else {
            showErrorToast("Failed to load input file.")
        }
    }
    
    function load_media(event: ProgressEvent<FileReader>){
        const uint8View = new Uint8Array(inputFileReader.result as ArrayBuffer)
        wasmModule.FS.writeFile(inputFileName, uint8View)
        let namePtr = wasmModule._Get_Input_Filename()
        wasmModule.stringToUTF8(inputFileName, namePtr, MAX_FILE_LEN + 1)
        loadedMediaType = wasmModule._Load_Media()

        // Show fields relevant to the media type
        if (loadedMediaType & Media_Type_Flags.IMAGE_TYPE) {
            $('.on-image').show()
        } else {
            $('.on-image').hide()
        }

        if (loadedMediaType & Media_Type_Flags.VIDEO_TYPE) {
            $('.on-video').show()
        } else {
            $('.on-video').hide()
        }

        if (loadedMediaType & Media_Type_Flags.AUDIO_TYPE) {
            $('.on-audio').show()
        } else {
            $('.on-audio').hide()
        }
    }

    // When media file is fully read, load the media file
    inputFileReader.addEventListener('loadend', load_media)

    // When image file changes, read the media file
    $('#image-file').on('change', read_media)

    // When width is changed and an image exists, adjust the height
    $('#grid-width').on('change', (_event) => {
        if (loadedMediaType != Media_Type_Flags.NONE_TYPE) {
            fitResolutionToImage(false, true)
        }
    })

    // When height is changed and an image exists, adjust the width
    $('#grid-height').on('change', (_event) => {
        if (loadedMediaType != Media_Type_Flags.NONE_TYPE) {
            fitResolutionToImage(true, false)
        }
    })

    // When fields change, rebuild blueprint simulation
    $('#generate-fieldset').on('change', async (_event) => {
        if ($('#generate-form').hasClass('was-validated')) {
            if (validateForm()) {
                await generateBlueprint()
            } else {
                clearBlueprintString()
            }
        }
    })

    // Show modal with blueprint string when requested
    $('#generate-btn').on('click', async (_event) => {
        if (lastBlueprintString) {
            showBlueprintString()
        } else if (validateForm()) {
            await generateBlueprint()
        } else {
            clearBlueprintString()
        }
    })
})()
