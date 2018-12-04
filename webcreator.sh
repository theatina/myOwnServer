#!/bin/bash

args=$#
correctnum=4


if [ "$args" -eq "$correctnum" ]; then


	dirname=$1
	if [ -d "$dirname" ]; then
    	echo -e "\n'$dirname' exists as a directory\n"
	else 
		echo -e "\nERROR - $dirname does not exist as a directory\n"
		exit 1
	fi

	if test "$(ls -A "$dirname")"; then
    	echo -e "\nWarning: directory is full, purging ...\n"
    	rm -rf "$dirname"/*/
	fi

	filename=$2
	if [ -e "$filename" ]; then
    	echo -e "\n'$filename' exists as a file\n"
	else 
		echo -e "\nERROR - $filename does not exist as a file\n"
		exit	
	fi

	w=$3
	if (( "$w" < 2 )); then
		echo -e "ERROR: websites must be at least 2 !"
		exit 1;
	fi

	p=$4
	if (( "$p" <= 1 )); then
		echo -e "ERROR: webpages must be at least 2 !"
		exit 1;
	fi

	digits='^[0-9]+$';
	if ! [[ $w =~ $digits ]] ; then
   		echo -e "\nERROR: w argument must be a number\n" ; exit 1
	fi

	if ! [[ $p =~ $digits ]] ; then
   		echo -e "\nERROR: p argument must be a number\n" ; exit 1
	fi

	lines=$(awk 'END {print NR}' $filename)
	echo -e "\nLines of '$filename' : $lines\n"


	if (( "$lines" >= 10000 )); then
    	n=0;
		arrpos=0;
		maxw=`expr "$w" - 1`;
		while [ "$n" -le "$maxw" ]; 
		do
			echo -e "\nCreating website $n ...\n"
			sitename="site$n"
	  		mkdir -p sites/"$sitename"

	  		count=0
			maxp=`expr "$p" - 1`;
	  		while [ "$count" -le "$maxp" ];      
			do
			  number=$RANDOM
			  name1="page$n"; 
			  name2="$number.html";
			  name=${name1}_${name2};
			  namefinal="$dirname/$sitename/$name"
			  arr[$arrpos]="$namefinal"
			  ltoall[$arrpos]=0;
			  echo -e "\nCreating page $dirname/$sitename/$name ...\n"
			  touch -- "$dirname/$sitename/$name";  
			  count=`expr "$count" + 1`;
			  arrpos=`expr "$arrpos" + 1`;
			done

	  		n=`expr "$n" + 1`;
		done

		links=`expr $w \* $p`;
		for i in "${arr[@]}"
		do
			echo "$i"
		done

		echo -e "\n"
		

		f=`expr $p / 2 + 1`
		echo -e "f = $f\n"
		if (( "$p" == 2 )); then
			f=1;
		
		fi

		q=`expr $w / 2 + 1`
		echo -e "q = $q\n"
		if (( "$w" == 2 )); then
			q=1;
		
		fi

		dirnum=0;
		for dir in "$dirname"/*; 
		do
			pagenum=0;
			ranlink=0;
			range=links-1;
			for file in "$dir"/*.html; 
			do

				#create array with f & q links
				fcounter=-1;
				qcounter=-1;
				
				#clear arrays
				keys=("${!farr[@]}")
				for l in "${keys[@]}"; 
				do 
					unset "farr[$l]"; 
				done

				keys=("${!qarr[@]}")
				for l in "${keys[@]}"; 
				do 
					unset "qarr[$l]"; 
				done
				
				

				telos=0;
				a=`expr $dirnum \* $p`;
				b=`expr $a + $p - 1`;

				echo "$a $b"

				#internal links
				r=0;
				while [ "$fcounter" -lt `expr $f - 1` ]; 
				do
					
					r=$(shuf -i ${a}-${b} -n 1)

					#echo -e "r = $r fcounter = $fcounter"

					tempfile="${arr[$r]}"
		
					if [ "$tempfile" != "$file" ]; then

						yparxei=0;
						for o in ${farr[@]} ; 
						do
							echo -e "\n--$o--$tempfile"
					        if [ "$o" == "$tempfile" ] ; then
					            yparxei=1;
					            echo "--yparxei!"
					        fi

					    done

						if [[ "$yparxei" -eq 1 ]]; then
    						
    						echo -e "\n$tempfile in farray!\n"
							
						else 
							echo "file = $file, tempfile = $tempfile"
							fcounter=`expr $fcounter + 1`
							farr[$fcounter]="$tempfile"
							ltoall[$r]=1;
							
						fi
						
					fi

				done



				printf 'FARR - %s\n' "${farr[@]}"


				#external links
				a2=0;
				b2=`expr $w \* $p - 1 `;
				ext=0;
				while [ "$qcounter" -lt `expr $q - 1` ]; 
				do
					
					ext=$(shuf -i ${a2}-${b2} -n 1);

					echo -e "ext = $ext qcounter = $qcounter"
				
		
					if (( "$ext" > "$b"  || "$ext" < "$a" )) ; then

						tempfile="${arr[$ext]}"
						yparxei=0;
						for o in ${qarr[@]} ; 
						do
							echo -e "\n--$o--$tempfile"
					        if [ "$o" == "$tempfile" ] ; then
					            yparxei=1;
					            echo "--yparxei!"
					        fi

					    done

						if [[ "$yparxei" -eq 1 ]]; then
    						
    						echo -e "\n$tempfile in qarray!\n"
							
						else 
							echo "file = $file, tempfile = $tempfile"
							qcounter=`expr $qcounter + 1`
							qarr[$qcounter]="$tempfile"
							ltoall[$ext]=1;
							
						fi
						
					fi

				done

				printf 'qARR - %s\n' "${qarr[@]}"

				echo -e "\nCreating content for page $file with $linestoprint lines starting at line $k...\n"
				
				lq=0;
				lf="$f";
				while  [ "$lq" -lt "$q" ] ; 
				do
					
					farr[$lf]="${qarr[$lq]}"
					lq=`expr $lq + 1`
					lf=`expr $lf + 1`

			    done

			    printf 'faRR FINAL - %s\n' "${farr[@]}"

				cp header.txt "$file"

				
				#creating webpage content
				floork=2
				rangek=`expr $lines - 2001`
				k=0
				while [ "$k" -le "$floork" ]
				do
				  k=$RANDOM
				  let "k %= $rangek"  # Scales $number down within $RANGE.
				done
				echo "Random number between $floork and $rangek ---  $k"

				floorm=1001
				rangem=1999
				m=0
				while [ "$m" -le "$floorm" ]
				do
				  m=$RANDOM
				  let "m %= $rangem"  # Scales $number down within $RANGE.
				done

				echo "Random number between $floorm and $rangem ---  $m"

				echo -e "\nk = $k m = $m\n"
				start="$k"
				let linestoprint=" $m / ($f + $q)";
				echo "lines to print $linestoprint"
				linkcount=0;
				for ((y=0; y< $f + $q; y++)); 
				do
					echo  "<br>PART $(($y + 1))-----------<br>" >> "$file"
					echo  "<br>" >> "$file"
					linestail=`expr $lines - $start`;
					cat "$filename" | tail -"${linestail}" | head -"${linestoprint}" >> "$file"
					echo  "<br>" >> "$file"

					echo  -e "\nAdding link to ${farr[$linkcount]}\n"
					timi=`expr $f - 1 `
					if [[ "$linkcount" -eq "$timi" ]]; then
    						
    					echo -e "\n...\n"
					fi	
		
					ttempath1="${farr[$linkcount]}"
					#echo "$ttempath"
					ttempath="${ttempath1:6}"
					echo "$ttempath"
					pagenametoprint="${ttempath:1}"
					while [[ ${pagenametoprint:0:1} != 'p' ]];
					do
					    pagenametoprint="${pagenametoprint:1}"
					done
					
					echo "$pagenametoprint"

					echo  -e "\n <br><a href=\"../$ttempath\">$pagenametoprint</a><br>\n" >> "$file"

					let linkcount="$linkcount + 1"
					let start="$start + $linestoprint"
				done
				

				cat footer.txt >> "$file"

				#cat "$file"

				echo -e "\n"

				pagenum=`expr $pagenum + 1`;
				
			done
			dirnum=`expr $dirnum + 1`;

		done

		linkstoall=1;
		for ((z=0; z < "$links"; z++)); 
		do
			#echo  -e "\nLink to all $z - ${ltoall[$z]}\n"

			if [[ "${ltoall[$z]}" -eq 0 ]]; then
    		
				linkstoall=0;
    		fi					

		done

		if [[ "$linkstoall" -eq 1 ]]; then
    						
    		echo -e "\nAll pages have at least one incoming link\n"
							
		fi		

		echo -e "\nDone\n"

	else
		echo -e "Lines < 10000 !!!"; exit 1;
	fi



else

	echo -e "\nWrong use of 'webcreator' ! ! \nUsage: ./webcreator.sh <directory> <textfile> <# of websites> <# of pages>\n"

fi

