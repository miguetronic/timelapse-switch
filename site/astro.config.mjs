// Timelapse Switch website (docs/decisions.md, ADR-010): English only, no cookies, no analytics.
import starlight from '@astrojs/starlight';
import { defineConfig } from 'astro/config';
import starlightLinksValidator from 'starlight-links-validator';

const repo = 'https://github.com/miguetronic/timelapse-switch';

export default defineConfig({
  site: 'https://miguetronic.github.io',
  base: '/timelapse-switch',
  integrations: [
    starlight({
      title: 'Timelapse Switch',
      description: 'Switch between real time and timelapse while you record in OBS Studio. Free and open source.',
      logo: { src: './src/assets/symbol.svg', alt: '' },
      favicon: '/favicon.svg',
      social: [
        { icon: 'github', label: 'GitHub', href: repo },
        { icon: 'heart', label: 'Sponsor', href: 'https://github.com/sponsors/miguetronic' },
      ],
      customCss: ['@fontsource-variable/instrument-sans', './src/styles/brand.css'],
      head: [
        { tag: 'meta', attrs: { property: 'og:image', content: 'https://miguetronic.github.io/timelapse-switch/social-preview.png' } },
        { tag: 'meta', attrs: { name: 'twitter:card', content: 'summary_large_image' } },
      ],
      sidebar: [
        { label: 'Start here', items: ['guide/install', 'guide/first-steps'] },
        { label: 'Using it', items: ['guide/hotkeys', 'guide/pause', 'guide/performance', 'guide/faq'] },
        { label: 'Messages', items: ['errors'] },
        { label: 'Project', items: ['support', 'about'] },
      ],
      editLink: { baseUrl: `${repo}/edit/main/site/` },
      // Error pages come from a dynamic route the validator cannot see; tools/check-site.sh checks them.
      plugins: [starlightLinksValidator({ errorOnLocalLinks: true, exclude: ['/timelapse-switch/errors/tsw-e*/'] })],
    }),
  ],
});
