from urllib.parse import urlparse, parse_qs

def has_scheme(uri: str) -> bool:
    """
    Checks whether the URI has a valid scheme.
    """
    parsed = urlparse(uri)
    return bool(parsed.scheme)

def parse_uri(uri: str) -> dict:
    """
    Parses a URI string if it is non-empty and contains a scheme.

    Args:
        uri (str): The URI to parse.

    Returns:
        dict: A dictionary with parsed components or empty dict if invalid.
    """
    if not uri:
        print("Empty URI string.")
        return {}

    if not has_scheme(uri):
        print("No scheme found in URI.")
        return {}

    result = urlparse(uri)

    parsed = {
        "scheme": result.scheme,
        "authority": result.netloc,
        "path": result.path,
        "query_raw": result.query,
        "query": {k: v if len(v) > 1 else v[0] for k, v in parse_qs(result.query).items()},
        "fragment": result.fragment,
    }

    return parsed
uris = [
    "resource://my/show/scene/file.usda?version=3&user=kim&user=lee",
    "://invalid.uri",
    "",
    "/just/path/no/scheme"
]

for u in uris:
    print(f"\nURI: {u}")
    result = parse_uri(u)
    print(result)