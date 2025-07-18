import { Media_Type_Flags, Image_Format_Flags, Factorio_Icon_Resolution } from './common';

declare module "display_gen" {
    // TODO: Is this needed??
    interface MyFS {
        readFile(path: string, opts?: { flags?: string | undefined }): Uint8Array
        writeFile(path: string, data: string | ArrayBufferView, opts?: { flags?: string | undefined }): void
        unlink(path: string): void
    }

    export interface MyModule extends EmscriptenModule {
        _Get_Input_Filename(): number /*(char *)*/
        _Set_Image_Config(width: number, height: number, tile_spacing: number, format_flags: Image_Format_Flags, icon_resolution: Factorio_Icon_Resolution): void
        _Set_Video_Config(framerate: number): void
        _Load_Media(): Media_Type
        _Process_Media(): boolean
        _Get_Blueprint_String(): number /*(char *)*/
        _Get_Preview_Filepath(): number /*(char *)*/
        FS: MyFS
        stringToUTF8(str: string, outPtr: number, maxBytesToRead?: number): void
        UTF8ToString(ptr: number, maxBytesToRead?: number): string
        canvas: HTMLCanvasElement
    }

    export default function Module(moduleOverrides?: Partial<MyModule>): Promise<MyModule>
}