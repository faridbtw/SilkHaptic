# Banner assets

`silkhaptic-banner.svg` contains the editable banner layout. The published README uses `silkhaptic-banner.png` so the controller image renders consistently on GitHub.

The red controller is Sony’s official **Volcanic Red DualSense** product image, downloaded from the [PlayStation DualSense product page](https://www.playstation.com/en-us/accessories/dualsense-wireless-controller/):

https://gmedia.playstation.com/is/image/SIEPDC/dualsense-volcanic-red-screenshot-01-en-04sep23?fmt=png-alpha&wid=1000

Controller image © Sony Interactive Entertainment. This third-party product image is not covered by the repository’s MIT software license. Its inclusion identifies the supported controller; SilkHaptic is an independent community project.

## Background removal

The downloaded product image contained an opaque white background. `dualsense-volcanic-red-cutout.png` is its transparent cutout, prepared with the built-in image generation tool in background-extraction edit mode. The original download is retained separately.

Prompt used:

> Use case: background-extraction. Edit target: the provided official Sony Volcanic Red DualSense product image. Remove ONLY its opaque white background and the broad floor shadow to produce a clean cutout with genuinely transparent alpha. Preserve the EXACT real controller: identical front-facing orientation, silhouette, proportions, red material, red touchpad, black center, all controls, PlayStation logo, triangle/circle/cross/square symbols, blue light strips, lighting and texture. Do not redraw, stylize, change colors, invent details, or add anything. Preserve the original landscape canvas and the controller's size and position on that canvas. Only white background pixels become transparent; retain clean accurate product edges. No backdrop, white rectangle, text, decoration or watermark.
