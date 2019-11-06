
library ieee;
use ieee.std_logic_1164.all;
use ieee.std_logic_unsigned.all;
use ieee.numeric_std.all;
use work.standards.all;

entity noc_ps_receiver is
	port (
		clock 			: in std_logic;
		reset 			: in std_logic;
		data_to_memory 	: out std_logic_vector(31 downto 0);
		valid			: out std_logic;
		consume			: in std_logic;
		rx				: in std_logic;
		data_in			: in regflit;
		credit_out		: out std_logic;
		
		--SDN configuration interface
		config_inport  : out std_logic_vector(2 downto 0);
		config_outport : out std_logic_vector(2 downto 0);
		config_valid   : out std_logic_vector(CS_SUBNETS_NUMBER - 1 downto 0);
		
		--Local Key configuration
		local_key_en   : in std_logic;
		local_key	   : in std_logic_vector(31 downto 0)
		
	);
end entity noc_ps_receiver;

architecture noc_ps_receiver of noc_ps_receiver is
	
	type send_buffer is array (1 downto 0) of regflit;
	signal data : send_buffer := (others=> (others=>'0'));
	signal full : std_logic_vector(1 downto 0);
	signal head, tail: std_logic_vector(0 downto 0);
	
	--Signal from SDN config
	type state is (header, p_size, ppayload, check_src, check_key, config, set_key);
	signal PS                    			: state;
	signal payload          				: regflit;
	signal k1, k2							: std_logic_vector(15 downto 0);
	signal en_config, en_set_key, key_valid	: std_logic;
	signal credit, rx_en					: std_logic;
	
begin
	
	credit <= (not full(0)) or (not full(1));
	credit_out <=  credit;
	valid <= full(0) or full(1);
	data_to_memory <= data(conv_integer(head)); 
	
	rx_en <= '0' when ((data_in(16) = '1' or data_in(17) = '1') and PS = header) or en_config = '1' or en_set_key = '1' else '1';
	key_valid <= '1' when (data_in(31 downto 16) xor k1) = k2 else '0';
	
	process(clock, reset)
	begin
		if reset = '1' then
			tail <= "0";
			head <= "0";
			full <= (others=>'0');
		elsif rising_edge(clock) then
			--reads from noc --- and mask a packet when a bit 16 or 17 of a header flits is on or enable config or enable set key is active
			if rx_en = '1' and rx = '1' and full( conv_integer(tail) ) = '0' then
			--if rx = '1' and full( conv_integer(tail) ) = '0' then --original read from noc condition
				full(conv_integer(tail)) <= '1';
				data(conv_integer(tail)) <= data_in;
				tail <= not tail;
			end if;
			
			--writes to memory
			if consume = '1' and full(conv_integer(head)) = '1' then
				full(conv_integer(head)) <= '0';
				head <= not head;
			end if;
		end if;
		
	end process;
	
	-- Configuracao e verificao de autenticidade de configuracao dos roteadores CS
--  header -> verifica-se o conteudo do header. Caso este possuir o bit 16 ativo, trata-se 
-- 			  de um pacote de configuracao de CS router. O reg en_config passa para 1 e a SM avanca para p_size.
--            Caso o header possuir o flit 17 ativo, trata-se de um pacote de configuracao de key. O reg en_set_key
--			  passa para 1 e a SM avanca para p_size.
--	p_size -> direciona a SM para o proximo estado apropriado, e tambem configura o reg de payload com o valor de payload do pacote
--  ppayload -> apenas consome o pacote ate chegar o ultimo flit, que faz com que a SM retorne para header concluindo o recebimento do pacote
--  check_src-> verifica se o 3o flit do pacote (contido em data_in) eh igua a zero. Por padrao so eh possivel configurar a key se o source do pacote
--				eh o PE no endereco 0x0. Caso o PE nao for 0x0, gera-se um report the warning e a SM avanca para ppayload para consumir o restante do pacote malicioso
--  check_key-> verifica se key_valid esta com um valor positivo. Caso afirmativo,
--              a SM avanca para config. Caso contratio, gera-se um report de warning e a SM avanca para ppayload para consumir o restante do pacote malicioso.              
--  config ->   configura o CS router. Esta configuracao eh feita pelo reg config_valid que somente eh ativado quando a SM esta em config. Apos a configuracao a SM volta para header.
--  set_key ->  configura a key de acordo com o conteudo de data_in. Apos a configuracao a SM volta para header.

	FMS_config : process(clock, reset)
	begin
		if reset = '1' then
			en_config <= '0';
			en_set_key <= '0';
			PS <= header;
			payload <= (others=> '0');
			
		elsif rising_edge(clock) then
			
			if rx = '1' and credit = '1' then
				case PS is
					when header =>
						if data_in(16) = '1' then
							en_config <= '1';
						elsif data_in(17) = '1' then
							en_set_key <= '1';
						end if;
						PS <= p_size;

					when p_size =>
						if en_config = '1' then
							PS <= check_key;
						elsif en_set_key = '1' then
							PS <= check_src;
						else
							PS <= ppayload;
						end if;
						payload <= data_in;
						
					when ppayload => --Just consumes the packet
						if payload = 1 then
							PS <= header;
							en_config <= '0';
							en_set_key <= '0';
						end if;
						payload <= payload - '1';
						
					when check_src =>
						if data_in = 0 then --Only accepts configuration from PE 0
							PS <= set_key;
						else
							report "Malicious config. detected (source different of 0x0)" severity warning;
							PS <= ppayload;
							payload <= payload - '1';
						end if;
						
					when check_key =>
						if key_valid = '1' then
							PS <= config;
						else
							report "Malicious config. detected (key does not match)" severity warning;
							PS <= ppayload;
							payload <= payload - '1';
						end if;

					when config =>
						en_config <= '0';
						PS <= header;
						
					when set_key =>
						en_set_key <= '0';
						PS <= header;
						
				end case;

			end if;
		end if;
	end process;
	
	--Updates the keys and the data used by CS router to configure the inport and outport
	config_update : process(clock, reset)
	begin
		if reset = '1' then
			config_valid <= (others=> '0');
			config_inport  <= (others=> '0');
			config_outport <= (others=> '0');
			k1 <= (others=> '0');
			k2 <= (others=> '0');
		elsif rising_edge(clock) then
			
			
			--Update CS routers config
			if PS = config then
				config_valid <= data_in(CS_SUBNETS_NUMBER downto 1);--Gets subnet information from config. packet flit
				config_inport  <= data_in(15 downto 13);
				config_outport <= data_in(12 downto 10);
			else 
				config_valid <= (others=> '0');
			end if;
		
		
		
			--Update keys
			if PS = set_key then
				k1 <= data_in(31 downto 16);
				k2 <= data_in(15 downto 0);
			elsif PS = check_key and key_valid = '1' then
				k1 <= k2;
				k2 <= data_in(15 downto 0) xor k2; --Extracts k3 using k2
			elsif local_key_en = '1' then
				k1 <= local_key(31 downto 16);
				k2 <= local_key(15 downto 0);
			end if;
		
		end if;
	end process;
	
	
end architecture noc_ps_receiver;