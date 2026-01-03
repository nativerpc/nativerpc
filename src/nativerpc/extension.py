##
#   Native RPC Python Language Extensions
#
#       parseInt
#       getHeaderMap
##

def parseInt(value):
    try:
        return int(value)
    except ValueError:
        pass
    return 0


def getHeaderMap(headers, names):
    result = {}
    headers = headers.lower()
    for name in names:
        hdr_name = name.lower() + ":"
        if hdr_name not in headers:
            result[name] = ""
            continue
        idx1 = headers.index(hdr_name) + len(hdr_name)
        idx2 = len(headers)
        try:
            idx2 = headers.index("\n", idx1)
        except Exception:
            pass
        result[name] = headers[idx1:idx2].strip()

    return result
