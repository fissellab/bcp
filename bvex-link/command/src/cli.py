from typing import Callable

import click
from click_shell import shell  # type: ignore


class CLI:
    def __init__(
        self,
        handle_set_bps: Callable[[int], None],
        handle_set_pkt_size: Callable[[int], None],
    ):
        self.handle_set_bps = handle_set_bps
        self.handle_set_pkt_size = handle_set_pkt_size
        self.app = self.create_app()

    def create_app(self):
        @shell(prompt="bvex-cli > ", intro="Bvex cli...")
        def app():
            pass

        @app.command()
        @click.argument("bits_per_second", type=int)
        def setbps(bits_per_second: int):
            self.handle_set_bps(bits_per_second)

        @app.command()
        @click.argument("pkt_size", type=int)
        def setpktsize(pkt_size: int):
            self.handle_set_pkt_size(pkt_size)

        # @app.command()
        # def getbps():
        #     click.echo(self.handle_get_bps())

        return app

    def run(self):
        self.app()


# if __name__ == "__main__":
#     bps = 1

#     def setbps(bps_):
#         global bps
#         bps = bps_

#     def getbps():
#         global bps
#         return bps

#     cli = CLI(setbps, getbps)
#     cli.run()
