import { DOMParser as XMLDOMParser, XMLSerializer as XMLDOMSerializer } from '@xmldom/xmldom';
import { def } from '../utils';

/**
 * `DOMParser` / `XMLSerializer` — pure-JS polyfills backed by `@xmldom/xmldom`.
 *
 * Added so demos that ship XML/HTML formats (Three's `ColladaLoader`,
 * `SVGLoader`, `X3DLoader`, `KTX2Loader` DFD parsing, etc.) can construct a
 * DOM tree at runtime. The full browser `DOMParser` also accepts
 * `text/html` and returns a mutable `Document` with layout-aware `body` /
 * `head` bindings; `xmldom` covers the XML side (`application/xml`,
 * `application/xhtml+xml`, `image/svg+xml`, `text/xml`) at ~40 KB
 * bundled — which is what every current demo actually asks for.
 */
export const DOMParser = XMLDOMParser as unknown as typeof globalThis.DOMParser;
export const XMLSerializer = XMLDOMSerializer as unknown as typeof globalThis.XMLSerializer;

def(DOMParser, 'DOMParser');
def(XMLSerializer, 'XMLSerializer');
