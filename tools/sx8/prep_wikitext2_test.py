"""prep_wikitext2_test.py -- wikitext-2-raw-v1 test split as one .txt, for llama-perplexity.

Same content as the author's prep_wikitext_txt.py (`"\\n".join(ds["text"])` over the test split),
but pulls the parquet straight from the hub so the `datasets` package is not needed.
"""
import io
import sys

import requests

URL = ("https://huggingface.co/datasets/Salesforce/wikitext/resolve/main/"
       "wikitext-2-raw-v1/test-00000-of-00001.parquet")

def main(out):
    import pyarrow.parquet as pq
    r = requests.get(URL, timeout=300)
    r.raise_for_status()
    t = pq.read_table(io.BytesIO(r.content))
    rows = t.column("text").to_pylist()
    text = "\n".join(rows)
    with open(out, "w", encoding="utf-8", newline="") as f:
        f.write(text)
    print("wikitext-2 test -> %s (%d rows, %d chars)" % (out, len(rows), len(text)))

if __name__ == "__main__":
    main(sys.argv[1])
