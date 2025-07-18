import { defineConfig } from 'vite'

export default defineConfig({
    plugins: [
        {
            name: 'configure-response-headers',
            configureServer(server) {
                server.middlewares.use((_req, res, next) => {
                    res.setHeader('Cross-Origin-Opener-Policy', 'same-origin')
                    res.setHeader('Cross-Origin-Embedder-Policy', 'require-corp')
                    next()
                })
            }
        }
      ],
    root: 'src',
    publicDir: '../public',
    build: {
        minify: true,
        outDir: '../dist',
        emptyOutDir: true
    },
    resolve: {
        alias: {
            "display_gen": "/../build/display_gen.js"
        },
    },
    server: {
        port: 8080,
        hot: true
    },
    base: '/vidtorio/'
})
