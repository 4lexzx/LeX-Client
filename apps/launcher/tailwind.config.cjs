/** @type {import('tailwindcss').Config} */
module.exports = {
  content: [
    './index.html',
    './src/**/*.{ts,tsx}',
    '../../packages/ui-components/src/**/*.{ts,tsx}',
    '../../packages/shared/src/**/*.{ts,tsx}',
    '../../packages/compatibility/src/**/*.{ts,tsx}',
    '../../packages/module-sdk/src/**/*.{ts,tsx}',
  ],
  theme: {
    extend: {
      colors: {
        surface: {
          0: '#101116',
          1: '#181A22',
          2: '#20232D',
          DEFAULT: '#20232D',
        },
        accent: {
          DEFAULT: '#00C8E8',
          violet: '#6C5CE7',
          fuchsia: '#D63384',
        },
        text: {
          primary: '#FFFFFF',
          secondary: '#A5A5C5',
          muted: '#7A7A9A',
        },
        danger: { DEFAULT: '#E5484D', fg: '#C33C38' },
      },
      fontFamily: {
        sans: ['Inter', 'Segoe UI Variable', 'Segoe UI', 'system-ui', 'sans-serif'],
        display: ['JetBrains Mono', 'Cascadia Code', 'Consolas', 'monospace'],
      },
    },
  },
  plugins: [],
};
